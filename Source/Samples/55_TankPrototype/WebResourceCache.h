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

/// \file

#pragma once

#include <Urho3D/Core/Object.h>
#include <Urho3D/AngelScript/ScriptFile.h>
#include <Urho3D/Resource/XMLElement.h>
#include <list>
#include <string>

using namespace Urho3D;

class URHO3D_API WebResourceCache : public Object {
URHO3D_OBJECT(WebResourceCache, Object);

public:
    /// Construct.
    explicit WebResourceCache(Context *context);

    /// Destruct. Free all resources.
    ~WebResourceCache() override;
    void AddToQueue(std::string filename, std::string content);
    void StartScripts();
    void StartSingleScript(const String& filename);
    String GetResourceContent(const String& filename);

private:
    void AddAngelScriptFile(const String& filename, const String& content);
    void AddLuaScriptFile(const String& filename, const String& content);
    void AddXMLFile(const String& filename, const String& content);
    void AddJSONFile(const String& filename, const String& content);
    void AddGLSLShader(const String& filename, const String& content);
    void AddMaterialFile(const String& filename, const XMLElement& source);
    void AddTechniqueFile(const String& filename, const String& content);
    void HandleUpdate(StringHash eventType, VariantMap& eventData);

    std::list<std::string> queue_;
    HashMap<String, SharedPtr<ScriptFile>> asScripts_;
};
