#include "CodeGenerator.h"
#include "Controller.h"

#include <QSqlQueryModel>
#include <QSqlError>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QTextStream>

namespace sg {

	struct CppMember {
		QString typeName;
		QString name;
		QString defaultValue;
	};

	struct CppStructure {
		QString name;
		QVector<CppMember> members;
	};

	static inline const QString TranslateTypeName(const CppMember& m) {

		if (m.typeName == "i8") return "int8_t";
		if (m.typeName == "u8") return "uint8_t";
		if (m.typeName == "i16") return "int16_t";
		if (m.typeName == "u16") return "uint16_t";
		if (m.typeName == "i32") return "int32_t";
		if (m.typeName == "u32") return "uint32_t";
		if (m.typeName == "i64") return "int64_t";
		if (m.typeName == "u64") return "uint64_t";
		if (m.typeName == "f32") return "float";
		if (m.typeName == "f64") return "double";
		if (m.typeName == "text") return "const char *";
		if (m.typeName == "component_ref") return QString("const struct %1*").arg(m.defaultValue);
		if (m.typeName == "component") return m.defaultValue;
		return "unknown";
	}

	static Result<> WriteHeader(const QVector<CppStructure>& structures, const QString& path) {

		qDebug() << "Writing" << path;

		QFile f(path);
		if (!f.open(QIODevice::WriteOnly|QIODevice::Text)) {
			return Error(QString("Unable to open '%1' for writing").arg(path));
		}

		QTextStream out(&f);

		out << "/* This code is all generated from SG Edit. Any edits to it may be lost.*/\n\n";
		out << "#pragma once\n\n";
		out << "#include <cstdint>\n\n";
		out << "#ifdef SG_BUILDER\n";
		out << "#include <vector>\n";
		out << "#endif\n";
		out << "namespace sg {\n\n";

		out << "\tenum class TypeId {\n";
		for (const auto& s : structures) {
			out << "\t\t" << s.name << ",\n";
		}
		out << "\t};\n";

		out << "\n";

		out << "\t#define SG_COMPONENTS";
		for (const auto& s : structures) {
			out << "\\\n\t\tX(" << s.name << ")";
		}

		out << "\n\n";

		for (const auto& s : structures) {
			out << "\n";
			out << "\tstruct alignas(16) " << s.name << " {\n";
			out << "\t\tstatic const TypeId StaticTypeId = TypeId::" << s.name << ";\n\n";

			for (const auto& m : s.members) {
				out << "\t\t" << TranslateTypeName(m) << " " << m.name << ";\n";
			}

			out << "\t};\n";
		}

		out << "\n";
		out << "\tstruct ComponentRange {\n";
		out << "\t\tTypeId typeId;\n";
		out << "\t\tuint32_t count;\n";
		out << "\t};\n\n";

		out << "\tstruct Scene {\n";
		out << "\t\tconst void **components;\n";
		out << "\t\tuintptr_t componentCount;\n\n";
		out << "\t\tconst ComponentRange *componentRanges;\n";
		out << "\t\tuintptr_t componentRangeCount;\n";
		out << "\t};\n";

		out << "\n\tconst Scene* ToScene(void *data);\n\n";
		out << "#ifdef SG_BUILDER\n";
		out << "\tconst std::vector<uint8_t> ToBytes(const Scene* scene);\n";
		out << "#endif\n";

		out << "}\n";

		return Ok();
	}

	static Result<> WriteSource(const QVector<CppStructure>& structures, const QString& cpp_path, const QString& header_path) {

		qDebug() << "Writing" << cpp_path;

		QFile f(cpp_path);
		if (!f.open(QIODevice::WriteOnly|QIODevice::Text)) {
			return Error(QString("Unable to open '%1' for writing").arg(cpp_path));
		}

		QTextStream out(&f);

		out << "/* This code is all generated from SG Edit. Any edits to it may be lost.*/\n\n";

		{
			QFileInfo cpp_path_info(cpp_path);
			QFileInfo header_path_info(header_path);

			out << "#include \"" << cpp_path_info.absoluteDir().relativeFilePath(header_path_info.absoluteFilePath()) << "\"\n\n";
		}

		out << "namespace sg {\n\n";
		out << "const Scene* ToScene(void *data) {\n\n";
		out << "#if SG_IMPL\n";
		out << "\tconst uintptr_t data_start = uintptr_t(data);\n\n";
		out << "\tScene *result = reinterpret_cast<Scene*>(data);\n";
		out << "\tresult->components = reinterpret_cast<const void**>(data_start + reinterpret_cast<uintptr_t>(result->components));\n";
		out << "\tresult->componentRanges = reinterpret_cast<const ComponentRange*>(data_start + reinterpret_cast<uintptr_t>(result->componentRanges));\n";
		out << "\tconst ComponentRange* r = result->componentRanges;\n";
		out << "\tconst ComponentRange* r_end = result->componentRanges + result->componentRangeCount;\n";
		out << "\tconst void** c = result->components;\n";

		for (const CppStructure& s: structures) {
			out << "\n\tif (r->typeId == TypeId::" << s.name << ") {\n";

			bool needs_fixup = false;

			for (const CppMember& m : s.members) {
				if (m.typeName == "component_ref") {

					if (!needs_fixup) {

						out << "\t\tfor (const void* c_end = c + r->count; c < c_end; ++c) {\n";

						out << "\t\t\t" << s.name << " *v = reinterpret_cast<" << s.name << "*>(c);\n";
						needs_fixup = true;
					}

					out << "\t\t\tv->" << m.name << " = reinterpret_cast<" << m.defaultValue << "*>(data_start + uintptr_t(v->" << m.name << "));\n";
				}
			}

			if (needs_fixup) {
				out << "\t\t}\n";
			} else {
				out << "\t\tc += r->count;\n";
			}

			out << "\t\tif (++r == r_end)\n";
			out << "\t\t\treturn result;\n";
			out << "\t}\n";

		}
		out << "\treturn result;\n";
		out << "#endif\nreturn nullptr;\n";

		out << "}\n";

		out << "\n#ifdef SG_BUILDER\n";
		out << "struct SceneSizes {\n";
		out << "\tsize_t scene_size;\n";
		out << "\tsize_t component_array_size;\n";
		out << "\tsize_t component_size;\n";
		out << "\tsize_t text_size;\n";
		out << "}\n";
		out << "SceneSizes CalcByteSize(const Scene *scene) {\n\n";
		out << "\tSceneSizes result;\n";
		out << "\tresult.scene_size = sizeof(Scene);\n";
		out << "\tresult.component_array_size + sizeof(DeferredPtr) * scene->component_count;\n";
		out << "\tresult.component_size = 0;\n";
		out << "\tresult.text_size = 0;\n\n";

		out << "\tfor (const DeferredPtr& p : *scene) {\n\n";
		out << "\t\tswitch (p.typeId) {\n";
		for (const CppStructure& s : structures) {
			out << "\n\t\t\tcase TypeId::" << s.name << " {\n";
			out << "\t\t\t\tresult.component_size += sizeof(" << s.name << ");\n";

			bool first = true;
			for (const CppMember& m : s.members) {
				if (m.typeName == "text") {

					if (first) {
						out << "\t\t\t\tconst " << s.name << " *c = reinterpret_cast<const " << s.name << "*>(p.ptr);\n";
					}


					out << "\t\t\t\tresult.text_size += strlen(c->" << m.name << ");\n";
				}
			}

			out << "\t\t\t} break;\n";
		}

		out << "\t\t}\n";
		out << "\t}\n";

		out << "\treturn result;\n";
		out << "}\n\n"; 

		out << "std::vector<uint8_t> ToBytes(const Scene *scene) {\n\n";
		out << "\tconst SceneSizes sizes = CalcByteSize(scene);\n";
		out << "\tstd::vector<uint8_t> result(sizes.scene_size + sizes.component_size + sizes.text_size);\n";
		out << "\tScene* scene_start = reinterpret_cast<Scene*>(result.data());\n";
		out << "\tDeferredPtr *out_p = reinterpret_cast<DeferredPtr*>(result.data() + sizes.scene_size);\n";
		out << "\tuint8_t* component_start = result.data() + sizes.scene_size + sizes.component_array_size;\n";
		out << "\tuint8_t* text_start = result.data() + sizes.text_size;\n";
		out << "\treinterpret_cast<uintptr_t&>(scene_start->components) = sizeof(Scene);\n";
		out << "\tscene_start->component_count = scene->component_count;\n";

		out << "\tfor (const DeferredPtr& in_p : scene) {\n";
		out << "\t\tDeferredPtr& out_p = ";
		out << "\t}\n";

		out << "}\n";

		out << "#endif\n";

		out << "}\n";

		return Ok();
	}

	int CalcDepth(const QMap<QString, QStringList>& deps, const QString& key) {
		if (deps.contains(key)) {

			int res = 0;
			for (const QString& s : deps[key]) {
				res = std::max(CalcDepth(deps, s), res);
			}

			if (res + 1 > deps.size())
				return -1;

			return res + 1;
		}

		return 0;
	}

	Result<> GenerateComponentFiles(const class Transaction& t, const QString& header_path, const QString& cpp_path) {

		qDebug() << "Generating" << header_path << cpp_path;

		QSqlQueryModel components;
		QString component_statement = QString("SELECT name, id FROM component ORDER BY id");
		components.setQuery(component_statement, *t.connection());
		if (components.lastError().isValid())
			return Error(components.lastError().text(), component_statement);

		QSqlQueryModel component_props;
		QString component_prop_statement = QString("SELECT name, type, default_value, component_id, id FROM component_prop ORDER BY component_id");
		component_props.setQuery(component_prop_statement, *t.connection());
		if (component_props.lastError().isValid())
			return Error(component_props.lastError().text(), component_prop_statement);

		// now we must process these results, write all of the headers

		qDebug() << "Parsing Database" << components.rowCount() << "Components," << component_props.rowCount() << "Members";

		QVector<CppStructure> structures;
		structures.reserve(components.rowCount());

		// this is a backwards map, what has a dependency of us
		QMap<QString, QStringList> dependencies;

		int prop_row = 0;
		for (int row = 0; row < components.rowCount(); ++row) {

			const int id = components.data(components.index(row, 1)).toInt();

			CppStructure s;
			s.name = components.data(components.index(row, 0)).toString();

			while (component_props.data(component_props.index(prop_row, 3)).toInt() == id) {

				CppMember m;
				m.name = component_props.data(component_props.index(prop_row, 0)).toString();
				m.typeName = component_props.data(component_props.index(prop_row, 1)).toString();
				m.defaultValue = component_props.data(component_props.index(prop_row, 2)).toString();

				if (m.typeName == "component") {
					dependencies[m.defaultValue].push_back(s.name);
				}

				s.members.push_back(std::move(m));

				++prop_row;
			}

			structures.push_back(std::move(s));
		}

		QMap<QString, int> dependency_depth;
		for (const CppStructure& s : structures) {
			int depth = CalcDepth(dependencies, s.name);

			if (depth == -1) {
				return Error(QString("Infinite dependency loop detected in '%1'").arg(s.name));
			}

			dependency_depth[s.name] = depth;
		}

		std::sort(structures.begin(), structures.end(), [&](const CppStructure& a, const CppStructure& b){
			const int a_depth = dependency_depth[a.name];
			const int b_depth = dependency_depth[b.name];

			if (a_depth == b_depth)
				return a.name < b.name;

			return a_depth > b_depth;
		});

		{
			auto res = WriteHeader(structures, header_path);
			if (res.failed())
				return res.error();
		}

		{
			auto res = WriteSource(structures, cpp_path, header_path);
			if (res.failed())
				return res.error();
		}

		return Ok();
	}
}
