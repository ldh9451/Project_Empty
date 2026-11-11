#include "cpp_example.h"
#include "voxel_terrain.h"
#include "entry_point.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

static void initialize_project_h(ModuleInitializationLevel level) {
    if (level == MODULE_INITIALIZATION_LEVEL_SCENE) {
        GDREGISTER_CLASS(GameInput);
        GDREGISTER_CLASS(GameCamera);
        GDREGISTER_CLASS(EntryPoint);
        GDREGISTER_CLASS(ProjectHCppExample);
        GDREGISTER_CLASS(ProjectHVoxelTerrain);
    }
}

static void uninitialize_project_h(ModuleInitializationLevel level) {}

extern "C" {
GDExtensionBool GDE_EXPORT project_h_library_init(
    GDExtensionInterfaceGetProcAddress get_proc_address,
    GDExtensionClassLibraryPtr library,
    GDExtensionInitialization *initialization) {
    GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
    init.register_initializer(initialize_project_h);
    init.register_terminator(uninitialize_project_h);
    init.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
}
