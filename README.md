# Urho3D - Dynamic Resource Cache subsystem

Subsystem allows to create, read or edit existing resources in the ResourceCache.


## How to build
Download all the files and place them in Urho3D project samples folder. Build project as usual with `-DURHO3D_SAMPLES=1` flag. 
Once finished there should be 55_DynamicResourceCache binary available.

## Usage
To use the subsystem outside the sample simply copy the following files to your app:

```bash
Source/Samples/55_DynamicResourceCache/DynamicResourceCache.h
Source/Samples/55_DynamicResourceCache/DynamicResourceCache.cpp
```

and create the subsystem

```c++
context->RegisterFactory<DynamicResourceCache>();
context_->RegisterSubsystem(new DynamicResourceCache(context_));
```

## Demo
Dynamic Resource Cache is currently used by the [Urho3D Tank](https://gitlab.com/luckeyproductions/tank) project.
Urho3D-Tank is a WEB IDE for Urho, it allows you to write code for the engine and see the changes in real time inside your browser.

See live demo [here](https://urho3d-tank.arnis.dev/)

[<img src="https://luckeyproductions.nl/tank/images/tank.png" width="100" height="100">](https://gitlab.com/luckeyproductions/tank)

## Todo:
* Binary file (models, images, etc.) dynamic loading
* Resource deletion
* LUA script support
* Save added resources to the filesystem and automatically load them next time app is started
