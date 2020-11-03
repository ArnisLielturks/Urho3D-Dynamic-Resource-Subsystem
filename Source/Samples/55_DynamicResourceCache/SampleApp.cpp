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

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Core/ProcessUtils.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/UI/Font.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/UI.h>

#ifdef URHO3D_ANGELSCRIPT
#include <Urho3D/AngelScript/Script.h>
#endif

#ifdef URHO3D_LUA
#include <Urho3D/LuaScript/LuaScript.h>
#endif

#include "SampleApp.h"
#include "DynamicResourceCache.h"

#include <Urho3D/DebugNew.h>

// Expands to this example's entry-point
URHO3D_DEFINE_APPLICATION_MAIN(SampleApp)

SampleApp::SampleApp(Context* context) :
    Sample(context)
{
    context->RegisterFactory<DynamicResourceCache>();
    // Register Dynamic Resource Cache as a subsystem
    context_->RegisterSubsystem(new DynamicResourceCache(context_));
}

void SampleApp::Start()
{
    SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(SampleApp, HandleUpdate));

    // Set the mouse mode to use in the sample
    Sample::InitMouseMode(MM_FREE);

    ResourceCache* cache = GetSubsystem<ResourceCache>();
    Graphics* graphics = GetSubsystem<Graphics>();
    Image* icon = cache->GetResource<Image>("Textures/UrhoIcon.png");
    graphics->SetWindowIcon(icon);
    graphics->SetWindowTitle("Dynamic Resource Cache");

#ifdef URHO3D_ANGELSCRIPT
    context_->RegisterSubsystem(new Script(context_));
#endif

#ifdef URHO3D_LUA
    context_->RegisterSubsystem(new LuaScript(context_));
#endif
}

void SampleApp::HandleUpdate(StringHash eventType, VariantMap& eventData)
{

}
