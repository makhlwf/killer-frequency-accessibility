#pragma once
#include "map_descriptions.h"
namespace mapreading {
static std::vector<std::string> discovered;
static size_t selected=0;
static std::vector<std::string> textures(Obj* root,bool descendants=true){
    std::vector<std::string> result;auto c=cls("UnityEngine.CoreModule.dll","UnityEngine","Renderer");if(!root||!c)return result;
    auto type=type_get_object(class_get_type(c));bool inactive=false;void* args[]={type,&inactive};
    auto renderers=descendants?array(invoke(root,"GetComponentsInChildren",2,args)):std::vector<Obj*>{getComponent(root,c)};
    for(auto renderer:renderers)if(renderer)for(auto material:array(invoke(renderer,"get_sharedMaterials"))){auto texture=invoke(material,"get_mainTexture");auto name=text(invoke(texture,"get_name"));if(!name.empty())result.push_back(name);}
    return result;
}
static bool read(const std::vector<std::string>& names){
    std::string output;
    for(const auto& entry:entries)if(std::find(names.begin(),names.end(),entry.texture)!=names.end()){
        if(std::find(discovered.begin(),discovered.end(),entry.texture)==discovered.end())discovered.push_back(entry.texture);
        if(!output.empty())output+=" ";
        output+=entry.description;
    }
    if(output.empty())return false;
    say(output);return true;
}
static void current(){
    navigation::cancel("Automatic walking stopped to read a clue.");
    auto p=navigation::player();if(!p||menuOpen()){say("Close the menu and inspect or face a map or clue first.");return;}
    auto inspect=field(p,"fsInspectHandler");auto held=field(inspect,"currentHoldable");
    if(held&&truth(invoke(inspect,"get_RestrictMovement"))&&read(textures(invoke(held,"get_gameObject"))))return;
    auto camera=field(p,"cameraFirstPersonTransform");if(!camera){say("The first person camera is unavailable.");return;}
    auto directionBox=invoke(camera,"get_forward");if(!directionBox)return;auto direction=*static_cast<navigation::Vec3*>(object_unbox(directionBox));navigation::RayHit hit{};
    if(navigation::cast(navigation::position(camera),direction,3.f,hit)){
        auto c=cls("UnityEngine.PhysicsModule.dll","UnityEngine","RaycastHit");auto boxed=value_box(c,&hit);auto collider=invoke(boxed,"get_collider");auto transform=invoke(collider,"get_transform");
        if(read(textures(invoke(transform,"get_gameObject"),false)))return;
        auto holdableClass=cls("Assembly-CSharp.dll","Interactables","Holdable");
        for(int i=0;transform&&i<3;++i){auto go=invoke(transform,"get_gameObject");if(getComponent(go,holdableClass)){if(read(textures(go)))return;break;}transform=invoke(transform,"get_parent");}
    }
    say("No supported map or clue is being inspected or faced within three metres. F6 can read visible text panels. Shift F12 repeats a previously discovered map.");
}
static void previous(){navigation::cancel("Automatic walking stopped to read a clue.");if(discovered.empty()){say("No maps or clues have been discovered with F12 in this session.");return;}selected=(selected+1)%discovered.size();read({discovered[selected]});}
}
