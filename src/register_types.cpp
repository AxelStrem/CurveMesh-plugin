#include "curve3d_mesh.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

#ifdef TOOLS_ENABLED
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/core/error_macros.hpp>
#endif

using namespace godot;

#ifdef TOOLS_ENABLED
namespace
{

void register_curve3d_mesh_editor_icon()
{
    Engine *engine = Engine::get_singleton();
    if (!engine || !engine->is_editor_hint())
    {
        return;
    }

    EditorInterface *editor_interface = EditorInterface::get_singleton();
    if (editor_interface == nullptr)
    {
        return;
    }

    Ref<Theme> editor_theme = editor_interface->get_editor_theme();
    if (editor_theme.is_null())
    {
        return;
    }

    ResourceLoader *loader = ResourceLoader::get_singleton();
    Ref<Texture2D> icon;
    if (loader != nullptr)
    {
        icon = loader->load(Curve3DMesh::get_class_icon_path());
    }

    if (icon.is_null())
    {
        WARN_PRINT("Curve3DMesh: failed to load editor icon resource.");
        return;
    }

    editor_theme->set_icon("Curve3DMesh", "EditorIcons", icon);
}

} // namespace
#endif

void initialize_curve3d_mesh_module(ModuleInitializationLevel p_level)
{
    switch (p_level)
    {
    case MODULE_INITIALIZATION_LEVEL_SCENE:
        ClassDB::register_class<Curve3DMesh>();
        break;
#ifdef TOOLS_ENABLED
    case MODULE_INITIALIZATION_LEVEL_EDITOR:
        register_curve3d_mesh_editor_icon();
        break;
#endif
    default:
        break;
    }
}

void uninitialize_curve3d_mesh_module(ModuleInitializationLevel p_level)
{
    switch (p_level)
    {
    case MODULE_INITIALIZATION_LEVEL_SCENE:
        break;
    default:
        break;
    }
}

extern "C"
{
    GDExtensionBool GDE_EXPORT curve3d_mesh_library_init(
        GDExtensionInterfaceGetProcAddress p_get_proc_address,
        GDExtensionClassLibraryPtr p_library,
        GDExtensionInitialization *r_initialization)
    {
        GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library,
                                                r_initialization);
        init_obj.register_initializer(initialize_curve3d_mesh_module);
        init_obj.register_terminator(uninitialize_curve3d_mesh_module);
        init_obj.set_minimum_library_initialization_level(
            MODULE_INITIALIZATION_LEVEL_SCENE);
        return init_obj.init();
    }
}
