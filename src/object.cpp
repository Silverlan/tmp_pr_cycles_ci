#include "pr_cycles/object.hpp"
#include "pr_cycles/mesh.hpp"
#include "pr_cycles/scene.hpp"
#include <render/object.h>
#include <render/scene.h>

using namespace pragma::modules;

#pragma optimize("",off)
cycles::PObject cycles::Object::Create(Scene &scene,Mesh &mesh)
{
	auto *object = new ccl::Object{}; // Object will be removed automatically by cycles
	object->mesh = *mesh;
	object->tfm = ccl::transform_identity();
	scene->objects.push_back(object);
	auto pObject = PObject{new Object{scene,*object,mesh}};
	scene.m_objects.push_back(pObject);
	return pObject;
}

cycles::Object::Object(Scene &scene,ccl::Object &object,Mesh &mesh)
	: WorldObject{scene},m_object{object},m_mesh{mesh.shared_from_this()}
{}

util::WeakHandle<cycles::Object> cycles::Object::GetHandle()
{
	return util::WeakHandle<cycles::Object>{shared_from_this()};
}

void cycles::Object::DoFinalize()
{
	m_mesh->Finalize();
	m_object.tfm = Scene::ToCyclesTransform(GetPose());
}

const cycles::Mesh &cycles::Object::GetMesh() const {return const_cast<Object*>(this)->GetMesh();}
cycles::Mesh &cycles::Object::GetMesh() {return *m_mesh;}

ccl::Object *cycles::Object::operator->() {return &m_object;}
ccl::Object *cycles::Object::operator*() {return &m_object;}
#pragma optimize("",on)
