//
// Copyright (c) 2008-2020 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <Urho3D/Core/Context.h>
#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Graphics/GraphicsDefs.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Shader.h>
#include <Urho3D/Graphics/Technique.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/IO/MemoryBuffer.h>
#include <Urho3D/IO/PackageFile.h>
#include <Urho3D/Resource/JSONFile.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Resource/XMLFile.h>
#include <Urho3D/Input/Input.h>
#include "WebResourceCache.h"

static WebResourceCache* resourceCacheObject = nullptr;

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

using namespace Urho3D;
using namespace emscripten;
static size_t AddResource(std::string filename, std::string content)
{
    if (resourceCacheObject) {
        resourceCacheObject->AddToQueue(filename, content);
        return content.length();
    }

    return 0;
}

int MultiplyArray(float factor, uintptr_t input, int length) {
    float* arr = reinterpret_cast<float*>(input);
    for (int i = 0; i <  length; i++) {
      arr[i] = factor * arr[i];
    }
    return 0;
}

static void LoadResourceList()
{
    if (resourceCacheObject) {
        val module = val::global("Module");
        auto packageFiles = resourceCacheObject->GetSubsystem<ResourceCache>()->GetPackageFiles();
        for (auto it = packageFiles.Begin(); it != packageFiles.End(); ++it) {
            auto files = (*it)->GetEntryNames();
            for (auto it2 = files.Begin(); it2 != files.End(); ++it2) {
                module.call<void>("ListResource", val((*it2).CString()));
            }
        }
    }
}

std::string GetResource(std::string filename)
{
    if (resourceCacheObject) {
        String content = resourceCacheObject->GetResourceContent(String(filename.c_str()));
        return content.CString();
    }

    return "";
}

void GetResourceBinary(std::string filename)
{
    if (resourceCacheObject) {
        resourceCacheObject->GetResourceContentBinary(String(filename.c_str()));
    }
}

void StartScripts()
{
    if (resourceCacheObject) {
        resourceCacheObject->StartScripts();
    }
}

void StartSingleScript(std::string filename)
{
    if (resourceCacheObject) {
        resourceCacheObject->StartSingleScript(String(filename.c_str()));
    }
}

EMSCRIPTEN_BINDINGS(ResourceModule) {
    function("AddResource", &AddResource);
    function("LoadResourceList", &LoadResourceList);
    function("StartScripts", &StartScripts);
    function("StartSingleScript", &StartSingleScript);
    function("GetResource", &GetResource);
    function("GetResourceBinary", &GetResourceBinary);
    function("MultiplyArray", &MultiplyArray, allow_raw_pointers());
}
#endif

WebResourceCache::WebResourceCache(Context* context):
        Object(context)
{
    resourceCacheObject = this;
    SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(WebResourceCache, HandleUpdate));
}

WebResourceCache::~WebResourceCache()
{
}

void WebResourceCache::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
//    auto input = GetSubsystem<Input>();
//    if (input->GetKeyPress(KEY_SPACE)) {
//        GetResourceContentBinary("Textures/Logo.png");
//    }
    if (queue_.size() >= 2) {
        using namespace Urho3D;
        String filename = queue_.front().c_str();
        queue_.pop_front();
        String content = queue_.front().c_str();
        queue_.pop_front();
        URHO3D_LOGINFOF("Processing queue item: %s", filename.CString());
        if (filename.EndsWith(".as")) {
            AddAngelScriptFile(filename, content);
        } else if (filename.EndsWith(".lua")) {
            AddLuaScriptFile(filename, content);
        } else if (filename.EndsWith(".xml")) {
            AddXMLFile(filename, content);
        } else if (filename.EndsWith(".json")) {
            AddJSONFile(filename, content);
        } else if (filename.EndsWith(".glsl")) {
            AddGLSLShader(filename, content);
        } else {
            URHO3D_LOGERRORF("Unable to process file %s, no handler implemented", filename.CString());
        }
    }
}

void WebResourceCache::AddAngelScriptFile(const String& filename, const String& content)
{
    SharedPtr<ScriptFile> file = SharedPtr<ScriptFile>(resourceCacheObject->GetSubsystem<ResourceCache>()->GetResource<ScriptFile>(filename));
    if (!file) {
        file = SharedPtr<ScriptFile>(new ScriptFile(context_));
        file->SetName(filename);
        GetSubsystem<ResourceCache>()->AddManualResource(file);
        URHO3D_LOGINFOF("Creating new manual AngelScript resource %s", filename.CString());
    }

    MemoryBuffer buffer((const void*) content.CString(), (unsigned) content.Length());
    bool loaded = file->Load(buffer);
    asScripts_[filename] = file.Get();

#ifdef __EMSCRIPTEN__
    if (loaded) {
        val module = val::global("Module");
        module.call<void>("FileLoaded", val(filename.CString()));
    } else {
        val module = val::global("Module");
        module.call<void>("FileLoadFailed", val(filename.CString()));
    }
#endif
}

void WebResourceCache::AddLuaScriptFile(const String& filename, const String& content)
{
    URHO3D_LOGERROR("Lua script dynamic loading is not yet supported!");
}

void WebResourceCache::AddXMLFile(const String& filename, const String& content)
{
    SharedPtr<XMLFile> file = SharedPtr<XMLFile>(new XMLFile(context_));
    MemoryBuffer buffer((const void*) content.CString(), (unsigned) content.Length());
    file->Load(buffer);
    if (file->GetRoot().GetName() == "material") {
        AddMaterialFile(filename, file->GetRoot());
    } else if (file->GetRoot().GetName() == "technique") {
        AddTechniqueFile(filename, content);
    } else {
        SharedPtr<XMLFile> file = SharedPtr<XMLFile>(resourceCacheObject->GetSubsystem<ResourceCache>()->GetResource<XMLFile>(filename));
        if (!file) {
            file = SharedPtr<XMLFile>(new XMLFile(context_));
            file->SetName(filename);
            GetSubsystem<ResourceCache>()->AddManualResource(file);
            URHO3D_LOGINFOF("Creating new manual XML resource %s", filename.CString());
        }

        MemoryBuffer buffer((const void*) content.CString(), (unsigned) content.Length());
        bool loaded = file->Load(buffer);

#ifdef __EMSCRIPTEN__
        if (loaded) {
            val module = val::global("Module");
            module.call<void>("FileLoaded", val(filename.CString()));
        } else {
            val module = val::global("Module");
            module.call<void>("FileLoadFailed", val(filename.CString()));
        }
#endif
    }
}

void WebResourceCache::AddJSONFile(const String& filename, const String& content)
{
    SharedPtr<JSONFile> file = SharedPtr<JSONFile>(resourceCacheObject->GetSubsystem<ResourceCache>()->GetResource<JSONFile>(filename));
    if (!file) {
        file = SharedPtr<JSONFile>(new JSONFile(context_));
        file->SetName(filename);
        GetSubsystem<ResourceCache>()->AddManualResource(file);
        URHO3D_LOGINFOF("Creating new manual AngelScript resource %s", filename.CString());
    }

    MemoryBuffer buffer((const void*) content.CString(), (unsigned) content.Length());
    bool loaded = file->Load(buffer);

#ifdef __EMSCRIPTEN__
    if (loaded) {
        val module = val::global("Module");
        module.call<void>("FileLoaded", val(filename.CString()));
    } else {
        val module = val::global("Module");
        module.call<void>("FileLoadFailed", val(filename.CString()));
    }
#endif
}

void WebResourceCache::AddTechniqueFile(const String& filename, const String& content)
{
    MemoryBuffer buffer((const void*) content.CString(), (unsigned) content.Length());
    SharedPtr<Technique> file = SharedPtr<Technique>(resourceCacheObject->GetSubsystem<ResourceCache>()->GetResource<Technique>(filename));
    if (!file) {
        file = SharedPtr<Technique>(new Technique(context_));
        file->SetName(filename);
        GetSubsystem<ResourceCache>()->AddManualResource(file);
        URHO3D_LOGINFOF("Creating new manual Material resource %s", filename.CString());
    }

    bool loaded = file->Load(buffer);

#ifdef __EMSCRIPTEN__
    if (loaded) {
        val module = val::global("Module");
        module.call<void>("FileLoaded", val(filename.CString()));
    } else {
        val module = val::global("Module");
        module.call<void>("FileLoadFailed", val(filename.CString()));
    }
#endif
}

void WebResourceCache::AddMaterialFile(const String& filename, const XMLElement& source)
{
    SharedPtr<Material> file = SharedPtr<Material>(resourceCacheObject->GetSubsystem<ResourceCache>()->GetResource<Material>(filename));
    if (!file) {
        file = SharedPtr<Material>(new Material(context_));
        file->SetName(filename);
        GetSubsystem<ResourceCache>()->AddManualResource(file);
        URHO3D_LOGINFOF("Creating new manual Material resource %s", filename.CString());
    }

    bool loaded = file->Load(source);

#ifdef __EMSCRIPTEN__
    if (loaded) {
        val module = val::global("Module");
        module.call<void>("FileLoaded", val(filename.CString()));
    } else {
        val module = val::global("Module");
        module.call<void>("FileLoadFailed", val(filename.CString()));
    }
#endif
}

void WebResourceCache::AddGLSLShader(const String& filename, const String& content)
{
    MemoryBuffer buffer((const void*) content.CString(), (unsigned) content.Length());
    SharedPtr<Shader> file = SharedPtr<Shader>(resourceCacheObject->GetSubsystem<ResourceCache>()->GetResource<Shader>(filename));
    if (!file) {
        file = SharedPtr<Shader>(new Shader(context_));
        file->SetName(filename);
        GetSubsystem<ResourceCache>()->AddManualResource(file);
        URHO3D_LOGINFOF("Creating new manual GLSL resource %s", filename.CString());
    }
    file->SetName(filename);

    bool loaded = file->Load(buffer);

#ifdef __EMSCRIPTEN__
    if (loaded) {
        val module = val::global("Module");
        module.call<void>("FileLoaded", val(filename.CString()));
    } else {
        val module = val::global("Module");
        module.call<void>("FileLoadFailed", val(filename.CString()));
    }
#endif
}

void WebResourceCache::AddToQueue(std::string filename, std::string content)
{
    queue_.push_back(filename);
    queue_.push_back(content);
}

void WebResourceCache::StartScripts()
{
    for (auto it = asScripts_.Begin(); it != asScripts_.End(); ++it) {
        if ((*it).second_->GetFunction("void Start()")) {
            URHO3D_LOGINFOF("Starting script %s", (*it).second_->GetName().CString());
            (*it).second_->Execute("void Start()");
        }
    }
}

void WebResourceCache::StartSingleScript(const String& filename)
{
    if (asScripts_.Contains(filename)) {
        if (asScripts_[filename]->GetFunction("void Start()")) {
            asScripts_[filename]->Execute("void Start()");
        }
    }
}

String WebResourceCache::GetResourceContent(const String& filename)
{
    auto cache = GetSubsystem<ResourceCache>();
    auto file = cache->GetFile(filename);
    String content;
    if (file) {
        while(!file->IsEof()) {
            content += file->ReadLine() + "\n";
        }
    }

    return content;
}

void* WebResourceCache::GetResourceContentBinary(const String& filename)
{
    auto cache = GetSubsystem<ResourceCache>();
    auto file = cache->GetFile(filename);
    if (file) {
        char buffer[file->GetSize()];
        int read = file->Read(buffer, file->GetSize());
        buffer_.SetData(buffer, file->GetSize());
        URHO3D_LOGINFOF(" WebResourceCache::GetResourceContentBinary Read %d bytes", read);
#ifdef __EMSCRIPTEN__
        uintptr_t pointer = reinterpret_cast<uintptr_t>(buffer_.GetData());
        val module = val::global("Module");
        module.call<void>("BinaryFileLoaded", val(filename.CString()), val(pointer), val(file->GetSize()));
#endif
    }
}
