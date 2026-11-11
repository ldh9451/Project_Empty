#include "entry_point.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void EntryPoint::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("Init"), &EntryPoint::Init);
    ClassDB::bind_method(D_METHOD("Update", "delta"), &EntryPoint::Update);
    ClassDB::bind_method(D_METHOD("Release"), &EntryPoint::Release);
    ClassDB::bind_method(D_METHOD("is_initialized"), &EntryPoint::is_initialized);
}

void EntryPoint::Init()
{
    if (initialized || !is_inside_tree() || Engine::get_singleton()->is_editor_hint())
        return;
    input = Object::cast_to<GameInput>(get_node_or_null("Input"));
    camera = Object::cast_to<GameCamera>(get_node_or_null("Camera"));
    if (!input || !camera)
    {
        UtilityFunctions::push_error("EntryPoint requires Input (GameInput) and Camera (GameCamera) children.");
        input = nullptr;
        camera = nullptr;
        set_process(false);
        return;
    }
    input->Init();
    camera->SetInput(input);
    camera->Init();
    initialized = true;
    set_process(true);
}

void EntryPoint::Update(double delta)
{
    if (!initialized)
        return;
    input->Update(delta);
    camera->Update(delta);
}

void EntryPoint::Release()
{
    if (!initialized)
        return;
    camera->Release();
    input->Release();
    camera = nullptr;
    input = nullptr;
    initialized = false;
    set_process(false);
}

void EntryPoint::_ready() { Init(); }
void EntryPoint::_process(double delta) { Update(delta); }
void EntryPoint::_exit_tree()
{
    Release();
    request_ready();
}
