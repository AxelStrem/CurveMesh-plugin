#!/usr/bin/env python

import os

from SCons.Script import Default, EnsurePythonVersion, EnsureSConsVersion, Environment, Export, SConscript

EnsureSConsVersion(4, 0)
EnsurePythonVersion(3, 8)

env = Environment()
env["custom_api_file"] = str(env.File("#/extension_api.json").abspath)
env["gdextension_dir"] = str(env.Dir("godot-cpp/gdextension").abspath)
env["build_library"] = True

Export("env")
env = SConscript("godot-cpp/SConstruct")

env.AppendUnique(CPPPATH=[env.Dir("src").srcnode()])
env.AppendUnique(CPPPATH=[
    env.Dir("godot-cpp/include").srcnode(),
    env.Dir("godot-cpp/gen/include").srcnode(),
    env.Dir("godot-cpp/gdextension").srcnode(),
])

sources = []
seen = set()
src_root = env.Dir("src").srcnode()
project_root = env.Dir("#").abspath
if os.path.isdir(src_root.abspath):
    for dirpath, _dirnames, filenames in os.walk(src_root.abspath):
        for filename in filenames:
            if filename == "doc_data.gen.cpp":
                continue
            if filename.endswith((".cc", ".cxx", ".cpp")):
                rel_path = os.path.relpath(os.path.join(dirpath, filename), project_root)
                norm_path = rel_path.replace("\\", "/")
                if norm_path not in seen:
                    seen.add(norm_path)
                    sources.append(env.File(norm_path))

if env["target"] in ["editor", "template_debug"]:
    doc_sources = env.Glob("doc_classes/*.xml")
    if doc_sources:
        doc_output_dir = os.path.join("src", "gen")
        os.makedirs(doc_output_dir, exist_ok=True)
        doc_target_path = os.path.join(doc_output_dir, "doc_data.gen.cpp").replace("\\", "/")
        doc_data = env.GodotCPPDocData(doc_target_path, source=doc_sources)
        doc_target_rel = os.path.relpath(env.File(doc_target_path).abspath, project_root).replace("\\", "/")
        seen.add(doc_target_rel)
        sources.append(doc_data)

if not sources:
    Default(env.Alias("curve3d_mesh_extension", []))
else:
    library_base = "curve3d_mesh" + env["suffix"]
    if env["platform"] == "ios":
        static_targets = [os.path.join("bin", library_base + env.get("LIBSUFFIX", ".a"))]
        library = env.StaticLibrary(target=static_targets, source=sources)
        env.NoCache(library)
        Default(library)
    else:
        dll_target = os.path.join("bin", library_base + env.get("SHLIBSUFFIX", ".dll"))
        implib_target = os.path.join("bin", library_base + env.get("LIBSUFFIX", ".lib"))
        library = env.SharedLibrary(target=[dll_target, implib_target], source=sources)
        env.NoCache(library)
        Default(library)

        plugin_dir = os.path.join("build", "addons", "curve3d_mesh")
        library_nodes = env.Flatten([library])
        installed_binaries = env.Install(plugin_dir, library_nodes)
        gdextension_target = os.path.join(plugin_dir, "curve3d_mesh.gdextension")
        gdextension_copy = env.InstallAs(gdextension_target, "curve3d_mesh.gdextension")

        Default(installed_binaries)
        Default(gdextension_copy)
