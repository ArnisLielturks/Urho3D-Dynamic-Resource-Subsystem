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
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/Technique.h>
#include <Urho3D/Graphics/Texture2D.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/IO/MemoryBuffer.h>
#include <Urho3D/IO/PackageFile.h>
#include <Urho3D/Resource/JSONFile.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Resource/XMLFile.h>
#include <Urho3D/Resource/XMLElement.h>
#include <Urho3D/IO/VectorBuffer.h>
#include <Urho3D/Network/HttpRequest.h>
#include <Urho3D/Network/Network.h>

#ifdef URHO3D_ANGELSCRIPT
#include <Urho3D/AngelScript/ScriptFile.h>
#endif

#include "DynamicResourceCache.h"

static DynamicResourceCache* resourceCacheObject = nullptr;

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>

using namespace Urho3D;
using namespace emscripten;
static size_t AddTextResource(std::string filename, std::string content)
{
    if (resourceCacheObject) {
        resourceCacheObject->ProcessResource(String(filename.c_str()), content.c_str(), content.length());
    }

    return 0;
}

void LoadResourceFromUrl(std::string url, std::string filename)
{
    if (resourceCacheObject) {
        resourceCacheObject->LoadResourceFromUrl(String(url.c_str()), String(filename.c_str()));
    }
}

void AddBinaryFile(std::string filename, intptr_t data, int length)
{
    if (resourceCacheObject) {
        resourceCacheObject->ProcessResource(String(filename.c_str()), reinterpret_cast<const char*>(data), length);
    }
}

void AddResourceFromBase64(std::string filename, std::string content)
{
    EM_ASM({
        let filename = UTF8ToString($0);
        let content = UTF8ToString($1);
        function convertDataURIToBinary(dataURI) {
            var BASE64_MARKER = ';base64,';
            var base64Index = dataURI.indexOf(BASE64_MARKER) + BASE64_MARKER.length;
            var base64 = dataURI.substring(base64Index);
            var raw = window.atob(base64);
            var rawLength = raw.length;
            var array = new Uint8Array(new ArrayBuffer(rawLength));

            for(i = 0; i < rawLength; i++) {
            array[i] = raw.charCodeAt(i);
            }
            return array;
        }

        data = convertDataURIToBinary(content);

        const nDataBytes = data.byteLength;
        const dataPtr = Module._malloc(nDataBytes);
        const dataHeap = new Uint8Array(Module.HEAPU8.buffer, dataPtr, nDataBytes);
        dataHeap.set(new Uint8Array(data));
        Module.AddBinaryFile(filename, dataHeap.byteOffset, nDataBytes);
        Module._free(dataHeap.byteOffset);
    }, filename.c_str(), content.c_str());
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
    function("AddTextResource", &AddTextResource);
    function("AddBinaryFile", &AddBinaryFile);
    function("AddResourceFromBase64", &AddResourceFromBase64);
    function("LoadResourceFromUrl", &LoadResourceFromUrl);
    function("LoadResourceList", &LoadResourceList);
    function("StartScripts", &StartScripts);
    function("StartSingleScript", &StartSingleScript);
    function("GetResource", &GetResource);
    function("GetResourceBinary", &GetResourceBinary);
}
#endif

DynamicResourceCache::DynamicResourceCache(Context* context):
Object(context)
        {
                resourceCacheObject = this;
        SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(DynamicResourceCache, HandleUpdate));
        }

DynamicResourceCache::~DynamicResourceCache()
{
}

void DynamicResourceCache::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
#ifdef URHO3D_NETWORK
    while (!remoteResources_.Empty()) {
        String url = remoteResources_.Front();
        remoteResources_.PopFront();
        String filename = remoteResources_.Front();
        remoteResources_.PopFront();

        NetworkResourceResponse response;
        response.first_ = filename;
        response.second_ = VectorBuffer();

        auto* network = GetSubsystem<Network>();
        httpRequests_.Push(NetworkResourceRequest(network->MakeHttpRequest(url), response));
        URHO3D_LOGINFOF("Loading remote resource %s from %s", filename.CString(), url.CString());
    }

    for (auto it = httpRequests_.Begin(); it != httpRequests_.End(); ++it) {
        if (it->first_) {
            if (it->first_->GetState() == HTTP_INITIALIZING) {
            } else if (it->first_->GetState() == HTTP_ERROR) {
                URHO3D_LOGERRORF("Failed to load resource from url due to error: %s", it->first_->GetError().CString());
                httpRequests_.Erase(it);
                break;
            } else if (it->first_->GetState() == HTTP_CLOSED){
                if (it->first_->GetAvailableSize() > 0) {
                    for (int i = 0; i < it->first_->GetAvailableSize(); i++) {
                        it->second_.second_.WriteByte(it->first_->ReadByte());
                    }
                } else if (it->second_.second_.GetSize() > 0){
                    String filename = it->second_.first_;
                    URHO3D_LOGINFOF("Remote resource %s downloaded from %s, size = %d", filename.CString(), it->first_->GetURL().CString(), it->second_.second_.GetSize());
                    ProcessResource(filename, (const char*)it->second_.second_.GetData(), it->second_.second_.GetSize());
                    if (filename.EndsWith(".as")) {
                        StartSingleScript(filename);
                    }
                    httpRequests_.Erase(it);
                    break;
                }
            }
        } else {
            httpRequests_.Erase(it);
        }
    }
#endif
}

void DynamicResourceCache::ProcessResource(const String& filename, const char* content, int size)
{
    if (filename.EndsWith(".as")) {
        AddAngelScriptFile(filename, String(content, size));
    } else if (filename.EndsWith(".lua")) {
        AddLuaScriptFile(filename, String(content, size));
    } else if (filename.EndsWith(".xml")) {
        AddXMLFile(filename, String(content, size));
    } else if (filename.EndsWith(".json")) {
        AddJSONFile(filename, String(content, size));
    } else if (filename.EndsWith(".glsl")) {
        AddGLSLShader(filename, String(content, size));
    } else if (filename.EndsWith(".mdl")) {
        AddModel(filename, content, size);
    } else if (IsImage(filename)) {
        AddImageFile(filename, content, size);
    } else if(filename.EndsWith(".js")) {
#ifdef __EMSCRIPTEN__
        emscripten_run_script(content);
#endif
    } else {
        URHO3D_LOGERRORF("Unable to process file %s, no handler implemented", filename.CString());
    }
}

bool DynamicResourceCache::IsImage(const String& filename)
{
    return filename.EndsWith(".dds")
           || filename.EndsWith(".jpg")
           || filename.EndsWith(".jpeg")
           || filename.EndsWith(".png")
           || filename.EndsWith(".icns");
}

void DynamicResourceCache::AddAngelScriptFile(const String& filename, const String& content)
{
#ifdef URHO3D_ANGELSCRIPT
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
#else
    URHO3D_LOGERROR("Engine built without AngelScript support!");
#endif
}

void DynamicResourceCache::AddLuaScriptFile(const String& filename, const String& content)
{
    URHO3D_LOGERROR("Lua script dynamic loading is not yet supported!");
}

void DynamicResourceCache::AddXMLFile(const String& filename, const String& content)
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

void DynamicResourceCache::AddJSONFile(const String& filename, const String& content)
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

void DynamicResourceCache::AddTechniqueFile(const String& filename, const String& content)
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

void DynamicResourceCache::AddMaterialFile(const String& filename, const XMLElement& source)
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

void DynamicResourceCache::AddGLSLShader(const String& filename, const String& content)
{
    MemoryBuffer buffer((const void*) content.CString(), (unsigned) content.Length());
    buffer.SetName(filename);
    SharedPtr<Shader> file = SharedPtr<Shader>(resourceCacheObject->GetSubsystem<ResourceCache>()->GetResource<Shader>(filename));
    if (!file) {
        file = SharedPtr<Shader>(new Shader(context_));
        file->SetName(filename);
        GetSubsystem<ResourceCache>()->AddManualResource(file);
        URHO3D_LOGINFOF("Creating new manual GLSL resource %s", filename.CString());
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

void DynamicResourceCache::AddImageFile(const String& filename, const char* content, int size)
{
    MemoryBuffer buffer((const void*) content, size);
    buffer.SetName(filename);
    SharedPtr<Texture2D> file = SharedPtr<Texture2D>(resourceCacheObject->GetSubsystem<ResourceCache>()->GetResource<Texture2D>(filename));
    if (!file) {
        file = SharedPtr<Texture2D>(new Texture2D(context_));
        file->SetName(filename);
        GetSubsystem<ResourceCache>()->AddManualResource(file);
        URHO3D_LOGINFOF("Creating new manual Material resource %s", filename.CString());
    }
    file->Load(buffer);
}

void DynamicResourceCache::AddModel(const String& filename, const char* content, int size)
{
    MemoryBuffer buffer((const void*) content, size);
    buffer.SetName(filename);
    SharedPtr<Model> file = SharedPtr<Model>(resourceCacheObject->GetSubsystem<ResourceCache>()->GetResource<Model>(filename));
    if (!file) {
        file = SharedPtr<Model>(new Model(context_));
        file->SetName(filename);
        GetSubsystem<ResourceCache>()->AddManualResource(file);
        URHO3D_LOGINFOF("Creating new manual Model resource %s, size=%d", filename.CString(), size);
    }
    file->Load(buffer);
}

void DynamicResourceCache::StartScripts()
{
#ifdef URHO3D_ANGELSCRIPT
    for (auto it = asScripts_.Begin(); it != asScripts_.End(); ++it) {
        if ((*it).second_->GetFunction("void Start()")) {
            URHO3D_LOGINFOF("Starting script %s", (*it).second_->GetName().CString());
            (*it).second_->Execute("void Start()");
        }
    }
#endif
}

void DynamicResourceCache::StartSingleScript(const String& filename)
{
#ifdef URHO3D_ANGELSCRIPT
    if (asScripts_.Contains(filename)) {
        if (asScripts_[filename]->GetFunction("void Start()")) {
            asScripts_[filename]->Execute("void Start()");
        }
    }
#endif
}

String DynamicResourceCache::GetResourceContent(const String& filename)
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

void* DynamicResourceCache::GetResourceContentBinary(const String& filename)
{
    auto cache = GetSubsystem<ResourceCache>();
    auto file = cache->GetFile(filename);
    if (file) {
        char buffer[file->GetSize()];
        int read = file->Read(buffer, file->GetSize());
        buffer_.SetData(buffer, file->GetSize());
        URHO3D_LOGINFOF(" DynamicResourceCache::GetResourceContentBinary Read %d bytes", read);
#ifdef __EMSCRIPTEN__
        uintptr_t pointer = reinterpret_cast<uintptr_t>(buffer_.GetData());
        val module = val::global("Module");
        module.call<void>("BinaryFileLoaded", val(filename.CString()), val(pointer), val(file->GetSize()));
#endif
    }

    return nullptr;
}

void DynamicResourceCache::LoadResourceFromUrl(const String& url, const String& filename)
{
#ifdef URHO3D_NETWORK
    remoteResources_.Push(url);
    remoteResources_.Push(filename);
#else
    URHO3D_LOGERROR("Engine built without network support!");
#endif
}
