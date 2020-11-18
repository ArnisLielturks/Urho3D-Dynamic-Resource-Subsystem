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

#pragma once

#include <Urho3D/Core/Object.h>
#include <list>
#include <string>
using namespace Urho3D;

namespace Urho3D {
    class ScriptFile;
    class XMLElement;
    class VectorBuffer;
    class HttpRequest;
    class VectorBuffer;
}

#ifdef URHO3D_NETWORK
/// HTTP request to handle remote resource loading.
typedef Pair<String, VectorBuffer> NetworkResourceResponse;
typedef Pair<SharedPtr<HttpRequest>, NetworkResourceResponse> NetworkResourceRequest;
#endif

/// Allows adding dynamic data to the resource cache.
class URHO3D_API DynamicResourceCache : public Object {
URHO3D_OBJECT(DynamicResourceCache, Object);

public:
    /// Construct.
    explicit DynamicResourceCache(Context *context);

    /// Destruct. Free all resources.
    ~DynamicResourceCache() override;
    /// Call Start() method to the dynamically loaded AngelScript files.
    void StartScripts();
    /// Start single AngelScript file.
    void StartSingleScript(const String& filename);
    /// Get textual resource data - XML,JSON, etc.
    String GetResourceContent(const String& filename);
    /// Get binary resource data - images, models, etc.
    void* GetResourceContentBinary(const String& filename);
    /// Load resource from url.
    void LoadResourceFromUrl(const String& url, const String& filename);
    /// Process single resource.
    void ProcessResource(const String& filename, const char* content, int size);

private:
    /// Add AngelScript file to the ResourceCache.
    void AddAngelScriptFile(const String& filename, const char* content, int size);
    /// Add LUA file to the ResourceCache.
    void AddLuaScriptFile(const String& filename, const char* content, int size);
    /// Add XML file to the ResourceCache.
    void AddXMLFile(const String& filename, const char* content, int size);
    /// Add JSON file to the ResourceCache.
    void AddJSONFile(const String& filename, const char* content, int size);
    /// Add GLSL file to the ResourceCache.
    void AddGLSLShader(const String& filename, const char* content, int size);
    /// Add Material file to the ResourceCache.
    void AddMaterialFile(const String& filename, const XMLElement& source);
    /// Add Techinque file to the ResourceCache.
    void AddTechniqueFile(const String& filename, const char* content, int size);
    /// Add Image file to the ResourceCache.
    void AddImageFile(const String& filename, const char* content, int size);
    /// Add model to ResourceCache.
    void AddModel(const String& filename, const char* content, int size);
    /// Handle queue data and add resources.
    void HandleUpdate(StringHash eventType, VariantMap& eventData);
    /// Checks if filename has image extension.
    bool IsImage(const String& filename);

    /// Remote resource queue.
    List<String> remoteResources_;
    #ifdef URHO3D_ANGELSCRIPT
    /// Custom .as script handler to support calling Start() method on them.
    HashMap<String, SharedPtr<ScriptFile>> asScripts_;
    #endif
    /// Buffer used to serve resource data to JS.
    VectorBuffer buffer_;
    #ifdef URHO3D_NETWORK
    /// HTTP request to handle remote resource loading.
    Vector<NetworkResourceRequest> httpRequests_;
    #endif
};
