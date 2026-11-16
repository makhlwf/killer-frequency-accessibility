#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <deque>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <set>
#include <regex>
#include <stdexcept>
#include "../vendor/minhook/include/MinHook.h"
#include "choice_state.h"
namespace fs = std::filesystem;
using Obj=void; using Class=void; using Method=void;
static fs::path base;
static std::mutex logMutex;
static void log(const std::string& s) { std::lock_guard<std::mutex> l(logMutex); std::ofstream f(base/"accessibility.log",std::ios::app); f<<GetTickCount64()<<" "<<s<<"\n"; }
static std::string utf8(const std::wstring& s) { if(s.empty()) return {}; int n=WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr); std::string r(n,0); WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),r.data(),n,nullptr,nullptr); return r; }
static std::wstring wide(const std::string& s) { if(s.empty()) return {}; int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0); std::wstring r(n,0); MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),r.data(),n);return r; }
static std::string clean(const std::string& s) {
    std::string out; bool tag=false,space=false;
    std::string markup;
    for(char c:s) {if(c=='<'){tag=true;markup.clear();continue;} if(c=='>'&&tag){tag=false;if(markup.rfind("sprite",0)==0){out+=" control icon ";}continue;} if(tag){markup+=c;continue;}
        if(c=='\r'||c=='\n'||c=='\t'||c==' '){if(!space&&!out.empty())out+=' ';space=true;}else{out+=c;space=false;}}
    for(auto [a,b]:{std::pair{"&amp;","&"}, {"&lt;","<"},{"&gt;",">"},{"&quot;","\""}}){size_t at=0;while((at=out.find(a,at))!=std::string::npos){out.replace(at,strlen(a),b);at+=strlen(b);}}
    return out;
}

// IL2CPP's public C API; all managed calls below run on the game's main thread.
static void* (*domain_get)();
static void** (*domain_get_assemblies)(void*,size_t*);
static void* (*assembly_get_image)(void*);
static const char* (*image_get_name)(void*);
static Class* (*class_from_name)(void*,const char*,const char*);
static Class* (*object_get_class)(Obj*);
static Class* (*class_get_parent)(Class*);
static const char* (*class_get_name)(Class*);
static const Method* (*class_get_method_from_name)(Class*,const char*,int);
static const Method* (*class_get_methods)(Class*,void**);
static const char* (*method_get_name)(const Method*);
static unsigned (*method_get_param_count)(const Method*);
static const void* (*method_get_param)(const Method*,unsigned);
static bool (*method_is_generic)(const Method*);
static char* (*type_get_name)(const void*);
static void (*ilfree)(void*);
static void* (*class_get_field_from_name)(Class*,const char*);
static const void* (*class_get_type)(Class*);
static Obj* (*type_get_object)(const void*);
static Obj* (*runtime_invoke)(const Method*,Obj*,void**,Obj**);
static void (*field_get_value)(Obj*,void*,void*);
static int (*string_length)(Obj*);
static const wchar_t* (*string_chars)(Obj*);
static Obj* (*string_new)(const char*);
static void* (*object_unbox)(Obj*);
static uintptr_t (*array_length)(Obj*);
static unsigned (*gchandle_new)(Obj*,bool);
static Obj* (*gchandle_get_target)(unsigned);
static void (*gchandle_free)(unsigned);
static void* (*thread_attach)(void*);
static void (*thread_detach)(void*);
static Obj* (*array_new)(Class*,uintptr_t);
static Obj* (*value_box)(Class*,void*);
static int (*field_get_offset)(void*);
static HMODULE game;
template<typename T> void api(T& target,const char* name) {target=reinterpret_cast<T>(GetProcAddress(game,name));if(!target)throw std::runtime_error(std::string("Missing API ")+name);}
static void bindAPI() {
#define API(x) api(x,"il2cpp_" #x)
    API(domain_get);API(domain_get_assemblies);API(assembly_get_image);API(image_get_name);API(class_from_name);API(object_get_class);API(class_get_parent);API(class_get_name);API(class_get_method_from_name);API(class_get_field_from_name);API(class_get_type);API(type_get_object);API(runtime_invoke);API(field_get_value);API(string_length);API(string_chars);API(string_new);API(object_unbox);API(array_length);API(gchandle_new);API(gchandle_get_target);API(gchandle_free);API(thread_attach);
    API(class_get_methods);API(method_get_name);API(method_get_param_count);API(method_get_param);API(method_is_generic);API(type_get_name);api(ilfree,"il2cpp_free");
    API(thread_detach);
    API(array_new);API(value_box);API(field_get_offset);
#undef API
}
static Class* cls(const char* assembly,const char* ns,const char* name) {
    size_t count=0;auto list=domain_get_assemblies(domain_get(),&count);
    for(size_t i=0;i<count;++i) {auto im=assembly_get_image(list[i]);if(strcmp(image_get_name(im),assembly)==0)return class_from_name(im,ns,name);}return nullptr;
}
static const Method* method(Class* c,const char* name,int n) {
    for(;c;c=class_get_parent(c)){
        void* iterator=nullptr;
        while(auto m=class_get_methods(c,&iterator)){
            if(strcmp(method_get_name(m),name)||method_get_param_count(m)!=(unsigned)n||method_is_generic(m))continue;
            // These APIs have same-arity string/bool overloads; require System.Type.
            if(n>0&&(!strcmp(name,"GetComponent")||!strcmp(name,"GetComponentsInChildren")||!strcmp(name,"FindObjectsOfType"))){
                auto type=type_get_name(method_get_param(m,0));bool correct=type&&!strcmp(type,"System.Type");if(type)ilfree(type);if(!correct)continue;
            }
            return m;
        }
    }return nullptr;
}
static Obj* call(const Method* m,Obj* o=nullptr,void** args=nullptr) {
    if(!m)return nullptr;
    Obj* exception=nullptr;auto r=runtime_invoke(m,o,args,&exception);if(exception){log("Managed exception invoking method");return nullptr;}return r;
}
static Obj* invoke(Obj* o,const char* name,int n=0,void** args=nullptr){return o?call(method(object_get_class(o),name,n),o,args):nullptr;}
static bool truth(Obj* value) {return value && *static_cast<bool*>(object_unbox(value));}
static std::string text(Obj* str) { if(!str)return {};int n=string_length(str);if(n<0||n>200000)return {};return clean(utf8(std::wstring(string_chars(str),n))); }
static Obj* field(Obj* o,const char* name) {if(!o)return nullptr;for(auto c=object_get_class(o);c;c=class_get_parent(c)){auto f=class_get_field_from_name(c,name);if(f){Obj* r=nullptr;field_get_value(o,f,&r);return r;}}return nullptr;}
static std::vector<Obj*> array(Obj* a) {std::vector<Obj*> out;if(!a)return out;size_t n=array_length(a);if(n>10000)return out;auto data=reinterpret_cast<Obj**>(static_cast<char*>(a)+32);out.assign(data,data+n);return out;}
static std::vector<std::string> strings(Obj* a) {std::vector<std::string> out;for(auto o:array(a))out.push_back(text(o));return out;}
static std::string componentText(Obj* component) {
    auto str=invoke(component,"get_text");if(!str)return {};
    int length=string_length(str);if(length<0||length>200000)return {};
    auto raw=utf8(std::wstring(string_chars(str),length));
    if(raw.find("<sprite")==std::string::npos)return clean(raw);
    auto asset=invoke(component,"get_spriteAsset");
    if(!asset){auto settings=cls("Unity.TextMeshPro.dll","TMPro","TMP_Settings");asset=call(method(settings,"get_defaultSpriteAsset",0));}
    static const std::regex sprite("<sprite[^>]*>");static const std::regex namePattern("name\\s*=\\s*[\\\"']?([^\\\"' >]+)");static const std::regex indexPattern("(?:index\\s*=|sprite\\s*=)\\s*[\\\"']?([0-9]+)");static const std::regex atlasPattern("^<sprite\\s*=\\s*[\\\"']([^\\\"']+)[\\\"']");
    std::string result;size_t at=0;
    for(std::sregex_iterator it(raw.begin(),raw.end(),sprite),end;it!=end;++it){auto match=*it;result+=raw.substr(at,match.position()-at);auto tag=match.str();std::smatch value;std::string name;
        auto selectedAsset=asset;std::smatch atlas;
        if(std::regex_search(tag,atlas,atlasPattern)){
            auto atlasName=atlas[1].str();if(text(invoke(selectedAsset,"get_name"))!=atlasName){selectedAsset=nullptr;
                auto c=cls("Unity.TextMeshPro.dll","TMPro","TMP_SpriteAsset");auto type=type_get_object(class_get_type(c));void* args[]={type};auto resources=cls("UnityEngine.CoreModule.dll","UnityEngine","Resources");
                for(auto candidate:array(call(method(resources,"FindObjectsOfTypeAll",1),nullptr,args)))if(text(invoke(candidate,"get_name"))==atlasName){selectedAsset=candidate;break;}
            }
        }
        if(std::regex_search(tag,value,namePattern))name=value[1].str();
        else if(selectedAsset&&std::regex_search(tag,value,indexPattern)){int index=atoi(value[1].str().c_str());auto table=invoke(selectedAsset,"get_spriteCharacterTable");auto count=invoke(table,"get_Count");if(count&&index>=0&&index<*static_cast<int*>(object_unbox(count))){void* args[]={&index};name=text(invoke(invoke(table,"get_Item",1,args),"get_name"));}}
        if(name=="ControlUnitAtlas_0")name="left mouse button";
        else if(name=="ControlUnitAtlas_1")name="right mouse button";
        else if(name=="ControlUnitAtlas_2")name="middle mouse button";
        else if(name=="ControlUnitAtlas_3")name="mouse movement";
        if(name.empty())name="control icon";
        else{for(auto& c:name)if(c=='_'||c=='-')c=' ';}
        result+=" "+name+" ";at=match.position()+match.length();
        static std::set<std::string> reported;if(reported.insert(tag+name).second)log("CONTROL ICON "+tag+" = "+name);
    }
    result+=raw.substr(at);return clean(result);
}

// A single speech worker owns Prism so game hooks never block on screen-reader IPC.
struct SpeechItem {std::string value; bool interrupt; unsigned epoch;};
static std::mutex speechMutex;
static std::deque<SpeechItem> speechQueue;
static unsigned speechEpoch=0;
static unsigned documentEpoch=0;
static bool muted=false,readSubtitles=false,manualSpeech=false,pauseChoiceTimers=true;
static void say(const std::string& value,bool interrupt=true) {
    if(value.empty()||(muted&&!manualSpeech))return;
    log("SPEECH "+value);
    std::lock_guard<std::mutex> lock(speechMutex);
    if(interrupt){++speechEpoch;speechQueue.clear();}
    if(speechQueue.size()<40)speechQueue.push_back({value,interrupt,speechEpoch});
}
static void stopSpeech() {std::lock_guard<std::mutex> lock(speechMutex);++speechEpoch;speechQueue.clear();speechQueue.push_back({"",true,speechEpoch});}
static DWORD WINAPI speechWorker(void*) {
    AddDllDirectory(base.c_str());
    HMODULE prism=LoadLibraryExW((base/L"prism.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    void* context=nullptr;void* backend=nullptr;
    void* (*init)(const void*)=nullptr;void* (*best)(void*)=nullptr;int (*speak)(void*,const char*,bool)=nullptr;int (*stop)(void*)=nullptr;const char* (*name)(void*)=nullptr;
    if(prism){init=reinterpret_cast<decltype(init)>(GetProcAddress(prism,"prism_init"));best=reinterpret_cast<decltype(best)>(GetProcAddress(prism,"prism_registry_create_best"));speak=reinterpret_cast<decltype(speak)>(GetProcAddress(prism,"prism_backend_speak"));stop=reinterpret_cast<decltype(stop)>(GetProcAddress(prism,"prism_backend_stop"));name=reinterpret_cast<decltype(name)>(GetProcAddress(prism,"prism_backend_name"));if(init&&best&&speak&&stop){context=init(nullptr);if(context)backend=best(context);}}
    HMODULE nvda=LoadLibraryExW((base/L"nvdaControllerClient64.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    auto nvSpeak=nvda?reinterpret_cast<int(WINAPI*)(const wchar_t*)>(GetProcAddress(nvda,"nvdaController_speakText")):nullptr;
    auto nvStop=nvda?reinterpret_cast<int(WINAPI*)()>(GetProcAddress(nvda,"nvdaController_cancelSpeech")):nullptr;
    if(backend)log(std::string("Speech backend: Prism / ")+(name?name(backend):"unknown"));else if(nvSpeak)log("Speech backend: NVDA controller fallback");else log("ERROR: no speech backend available");
    while(true){SpeechItem item;bool found=false;{
        std::lock_guard<std::mutex> lock(speechMutex);if(!speechQueue.empty()){item=std::move(speechQueue.front());speechQueue.pop_front();found=item.epoch==speechEpoch;}}
        if(found){int result=-1;if(backend){if(item.value.empty())result=stop(backend);else result=speak(backend,item.value.c_str(),item.interrupt);}if(result!=0&&nvSpeak){if(item.interrupt&&nvStop)nvStop();if(!item.value.empty())result=nvSpeak(wide(item.value).c_str());else result=0;}if(result)log("Speech output error "+std::to_string(result));}Sleep(10);
    }
}

static ChoiceState choices;
static unsigned callbackHandle=0,questionOwnerHandle=0;
static unsigned currentWorldUiHandle=0;
static std::string localized(Obj* world,Obj* key){
    auto raw=text(key);if(raw.empty())return {};
    void* args[]={key};auto translated=invoke(world,"LocaliseText",1,args);auto result=text(translated);
    if(result.empty()||result.rfind("No translation found",0)==0)return raw;
    return result;
}
static std::string lastSubtitle,lastDocument,lastPrompt,lastFocus,lastContent;
static uintptr_t lastSelected=0;
static unsigned long long lastPump=0;
static Class *eventClass=nullptr,*tmpClass=nullptr,*textClass=nullptr,*selectableClass=nullptr,*objectClass=nullptr;
static std::string label(Obj* go) {
    if(!go)return {};
    std::string result;
    for(auto c:{tmpClass,textClass}) {if(!c)continue;auto type=type_get_object(class_get_type(c));bool includeInactive=false;void* args[]={type,&includeInactive};
        for(auto component:array(invoke(go,"GetComponentsInChildren",2,args))){auto part=componentText(component);if(!part.empty()){if(!result.empty())result+=". ";result+=part;}}}
    if(result.empty())result=text(invoke(go,"get_name"));
    return result;
}
static Obj* eventSystem() {return eventClass?call(method(eventClass,"get_current",0)):nullptr;}
static Obj* focusedMenu() {
    auto c=cls("Assembly-CSharp.dll","MenuSystem","Menu");if(!c)return nullptr;
    auto type=type_get_object(class_get_type(c));void* args[]={type};
    for(auto menu:array(call(method(objectClass,"FindObjectsOfType",1),nullptr,args)))if(truth(invoke(menu,"get_hasFocus")))return invoke(menu,"get_gameObject");
    return nullptr;
}
static bool menuOpen() {return focusedMenu()!=nullptr;}
static bool questionVisible() {auto owner=callbackHandle&&questionOwnerHandle?gchandle_get_target(questionOwnerHandle):nullptr;auto go=owner?field(owner,"dialogTransform"):nullptr;if(go)go=invoke(go,"get_gameObject");return choices.active&&owner&&go&&truth(invoke(go,"get_activeInHierarchy"));}
static void clearChoice() {choices.clear();if(callbackHandle){gchandle_free(callbackHandle);callbackHandle=0;}if(questionOwnerHandle){gchandle_free(questionOwnerHandle);questionOwnerHandle=0;}}
static std::vector<Obj*> controls(Obj* root) {
    std::vector<Obj*> candidates;if(!root)return candidates;
    auto t=type_get_object(class_get_type(selectableClass));bool inactive=false;void* args[]={t,&inactive};
    for(auto s:array(invoke(root,"GetComponentsInChildren",2,args))) {
        auto go=invoke(s,"get_gameObject");if(truth(invoke(go,"get_activeInHierarchy"))&&truth(invoke(s,"IsInteractable")))candidates.push_back(go);
    }
    return candidates;
}
static Obj* getComponent(Obj* go,Class* c){if(!go||!c)return nullptr;auto type=type_get_object(class_get_type(c));void* args[]={type};return invoke(go,"GetComponent",1,args);}
static void menuMove(int delta) {
    auto es=eventSystem();if(!es)return;
    auto candidates=controls(focusedMenu());
    if(candidates.empty()){say("No available menu controls.");return;}
    auto current=invoke(es,"get_currentSelectedGameObject");auto selected=getComponent(current,selectableClass);
    auto neighbor=invoke(selected,delta>0?"FindSelectableOnDown":"FindSelectableOnUp");auto destination=invoke(neighbor,"get_gameObject");
    if(std::find(candidates.begin(),candidates.end(),destination)==candidates.end()){
        auto it=std::find(candidates.begin(),candidates.end(),current);int i=it==candidates.end()?(delta>0?-1:0):(int)(it-candidates.begin());i=(i+delta+(int)candidates.size())%(int)candidates.size();destination=candidates[i];
    }
    void* p[]={destination};invoke(es,"SetSelectedGameObject",1,p);say(label(destination));
}
static void syncChoiceFocus(bool toGame) {
    if(!choices.active||!questionOwnerHandle)return;
    auto owner=gchandle_get_target(questionOwnerHandle);auto root=field(owner,"answerRoot");if(root)root=invoke(root,"get_gameObject");
    auto list=controls(root);auto es=eventSystem();auto selected=invoke(es,"get_currentSelectedGameObject");
    // ShowDialogueChoice instantiates answers in array order. Unity returns these
    // controls in hierarchy order; require the full set, never guess by text.
    if(list.size()!=choices.answers.size()){log("Answer control mapping not ready");return;}
    if(toGame){void* p[]={list[choices.selected]};invoke(es,"SetSelectedGameObject",1,p);return;}
    auto found=std::find(list.begin(),list.end(),selected);
    if(found!=list.end()){int i=(int)(found-list.begin());if(choices.selected!=i){choices.selected=i;say(choices.current());}}
}
static void screen() {
    std::string result;auto root=focusedMenu();std::vector<std::pair<std::vector<int>,std::string>> content;std::set<Obj*> seen;
    for(auto c:{tmpClass,textClass}) {if(!c)continue;auto t=type_get_object(class_get_type(c));bool inactive=false;void* args[]={t,&inactive};
        auto components=root?invoke(root,"GetComponentsInChildren",2,args):call(method(objectClass,"FindObjectsOfType",1),nullptr,args);
        for(auto component:array(components)){if(!seen.insert(component).second)continue;auto go=invoke(component,"get_gameObject");if(!truth(invoke(go,"get_activeInHierarchy")))continue;auto s=componentText(component);if(s.empty())continue;
            std::vector<int> key;auto transform=invoke(component,"get_transform");for(int depth=0;transform&&depth<64;++depth){auto boxed=invoke(transform,"GetSiblingIndex");key.push_back(boxed?*static_cast<int*>(object_unbox(boxed)):0);transform=invoke(transform,"get_parent");}std::reverse(key.begin(),key.end());content.push_back({key,s});
        }}
    std::stable_sort(content.begin(),content.end(),[](const auto& a,const auto& b){return a.first<b.first;});
    for(const auto& entry:content){result+=entry.second+". ";if(result.size()>16000){result+=" More text is available by interacting with individual objects.";break;}}
    say(result.empty()?"No active screen text found.":result);
}
#include "navigation.h"
#include "map_reader.h"
static void activate() {
    if(choices.active&&!menuOpen()) {
        if(!questionVisible()){clearChoice();say("That question is no longer available.");return;}
        syncChoiceFocus(false);
        auto owner=gchandle_get_target(questionOwnerHandle);auto root=field(owner,"answerRoot");if(root)root=invoke(root,"get_gameObject");
        if(controls(root).size()!=choices.answers.size()){say("Answer controls are not ready. Try again or use the game's answer controls.");return;}
        auto callback=gchandle_get_target(callbackHandle);auto index=choices.selected;
        if(!callback||index<0||index>=(int)choices.ids.size())return;
        auto id=string_new(choices.ids[index].c_str());auto answer=choices.answers[index];
        unsigned saved=gchandle_new(callback,false);auto generation=choices.generation;void* args[]={id};
        auto cb=gchandle_get_target(saved);auto m=method(object_get_class(cb),"Invoke",1);Obj* exception=nullptr;
        if(m)runtime_invoke(m,cb,args,&exception);
        gchandle_free(saved);
        if(!m||exception){say("Could not select that answer. Try again.");log("ERROR answer callback failed");return;}
        if(choices.generation==generation)clearChoice();
        say("Selected. "+answer,!choices.active);log("Answer callback submitted");
    } else {
        if(!menuOpen()&&navigation::interact())return;
        auto es=eventSystem();auto go=invoke(es,"get_currentSelectedGameObject");auto candidates=controls(focusedMenu());
        if(!go||std::find(candidates.begin(),candidates.end(),go)==candidates.end()){say("No menu control selected. Use F3 or F4 first.");return;}
        auto buttonClass=cls("UnityEngine.UI.dll","UnityEngine.UI","Button");auto type=type_get_object(class_get_type(buttonClass));void* args[]={type};auto button=invoke(go,"GetComponent",1,args);
        if(button&&truth(invoke(button,"IsInteractable")))invoke(button,"Press");else say("Use the game's controls to change this setting.");
    }
}
static void command(const std::string& cmd) {
    struct Manual {Manual(){manualSpeech=true;}~Manual(){manualSpeech=false;}} manual;
    log("COMMAND "+cmd);
    if(cmd=="repeat"&&(navigation::moving||navigation::planner||navigation::mouseHeld)){navigation::announce();return;}
    if(choices.active&&!menuOpen())syncChoiceFocus(false);
    if(cmd=="help")say("Killer Frequency accessibility. F1 help. F2 repeat. F3 previous answer or menu control. F4 next. F5 select, interact with the targeted object, or try the nearby door. F6 read screen text. F7 toggle spoken subtitles. F8 stop speech and automatic walking. F9 toggle automatic speech. F10 toggle choice timer pause. F11 walk to the selected destination, or stop walking. Shift F11 next destination, Control F11 previous destination. Alt F11 announce destination. F12 read the map or clue you are inspecting. Shift F12 recall discovered clues.");
    else if(cmd=="repeat") {if(choices.active&&!menuOpen()&&questionVisible())say(choices.all()+" Reviewing "+choices.current());else {auto s=label(invoke(eventSystem(),"get_currentSelectedGameObject"));if(!s.empty())say(s);else if(!lastContent.empty())say(lastContent);else screen();}}
    else if(cmd=="previous"||cmd=="next") {if(choices.active&&!menuOpen()&&questionVisible()){choices.move(cmd=="next"?1:-1);syncChoiceFocus(true);say(choices.current());}else menuMove(cmd=="next"?1:-1);}
    else if(cmd=="activate")activate();
    else if(cmd=="screen")screen();
    else if(cmd=="subtitles"){readSubtitles=!readSubtitles;say(readSubtitles?"Spoken subtitles on.":"Spoken subtitles off. Voiced dialogue remains audible.");}
    else if(cmd=="stop"){navigation::cancel("Automatic walking cancelled.");stopSpeech();}
    else if(cmd=="mute"){muted=!muted;say(muted?"Automatic speech off. Manual reading remains available.":"Automatic speech on.");}
    else if(cmd=="timers"){pauseChoiceTimers=!pauseChoiceTimers;say(pauseChoiceTimers?"Choice timers paused while answers are available.":"Normal choice timers. Answers may expire.");}
    else if(cmd=="snapshot"){screen();log("SNAPSHOT choice="+std::to_string(choices.active)+" focus="+lastFocus);}
    else if(cmd=="navdump")navigation::dump();
    else if(cmd=="navgo")navigation::start();
    else if(cmd=="navnext")navigation::cycle(1);
    else if(cmd=="navprevious")navigation::cycle(-1);
    else if(cmd=="navstatus")navigation::announce();
    else if(cmd=="map")mapreading::current();
    else if(cmd=="maprepeat")mapreading::previous();
}
static void pump() {
    auto now=GetTickCount64();if(now-lastPump<65)return;lastPump=now;
    HWND foreground=GetForegroundWindow();DWORD pid=0;GetWindowThreadProcessId(foreground,&pid);bool focused=pid==GetCurrentProcessId();
    if(choices.active&&!questionVisible()){clearChoice();stopSpeech();}
    static bool held[10]{},wasFocused=false;
    for(int i=0;i<10;++i){bool down=(GetAsyncKeyState(VK_F1+i)&0x8000)!=0;const char* cmds[]={"help","repeat","previous","next","activate","screen","subtitles","stop","mute","timers"};if(focused&&wasFocused&&down&&!held[i])command(cmds[i]);held[i]=down;}
    bool continuingFocus=wasFocused;
    wasFocused=focused;
    static bool navigationHeld=false;bool navigationDown=(GetAsyncKeyState(VK_F11)&0x8000)!=0;
    if(focused&&continuingFocus&&navigationDown&&!navigationHeld){if(GetAsyncKeyState(VK_SHIFT)&0x8000)command("navnext");else if(GetAsyncKeyState(VK_CONTROL)&0x8000)command("navprevious");else if(GetAsyncKeyState(VK_MENU)&0x8000)command("navstatus");else command("navgo");}navigationHeld=navigationDown;
    static bool mapHeld=false;bool mapDown=(GetAsyncKeyState(VK_F12)&0x8000)!=0;if(focused&&continuingFocus&&mapDown&&!mapHeld)command((GetAsyncKeyState(VK_SHIFT)&0x8000)?"maprepeat":"map");mapHeld=mapDown;
    if(focused){
        if(choices.active&&!menuOpen())syncChoiceFocus(false);
        auto selected=invoke(eventSystem(),"get_currentSelectedGameObject");auto description=label(selected);
        if(!description.empty()&&(reinterpret_cast<uintptr_t>(selected)!=lastSelected||description!=lastFocus)){
            lastSelected=reinterpret_cast<uintptr_t>(selected);lastFocus=description;if(!choices.active||menuOpen())say(description);
        }
    }
    // Local diagnostic commands; only this process's main thread executes managed operations.
    static unsigned long long lastFile=0;if(now-lastFile>250){lastFile=now;auto path=base/"command.txt";std::ifstream f(path);std::string cmd;if(f&&std::getline(f,cmd)){f.close();std::error_code ec;fs::remove(path,ec);command(cmd);}}
}

static void readWorldHints(Obj* world) {
    static unsigned long long last=0;auto now=GetTickCount64();if(now-last<400)return;last=now;
    DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);if(pid!=GetCurrentProcessId()||choices.active||menuOpen())return;
    std::string hint;
    auto tutorialClass=cls("Assembly-CSharp.dll","","TutorialAnchor");
    if(tutorialClass){auto type=type_get_object(class_get_type(tutorialClass));void* args[]={type};for(auto tutorial:array(call(method(objectClass,"FindObjectsOfType",1),nullptr,args))){
        auto group=field(tutorial,"_tutorialCanvasGroup");auto alpha=invoke(group,"get_alpha");if(!alpha||*static_cast<float*>(object_unbox(alpha))<0.1f)continue;
        auto transform=field(tutorial,"_tutorial");auto go=invoke(transform,"get_gameObject");if(go&&truth(invoke(go,"get_activeInHierarchy"))){auto s=label(go);if(!s.empty())hint+=s+". ";}
    }}
    auto manager=field(world,"_interactableUIManager");auto group=field(manager,"_canvasGroup");auto alpha=invoke(group,"get_alpha");
    if(manager&&!truth(invoke(manager,"get_Hidden"))&&alpha&&*static_cast<float*>(object_unbox(alpha))>0.1f){auto s=label(invoke(manager,"get_gameObject"));if(!s.empty())hint+=s;}
    if(hint!=lastPrompt){lastPrompt=hint;if(!hint.empty()){lastContent=hint;say(hint);}}
}

using ShowChoice=void(*)(Obj*,Obj*,Obj*,Obj*,Obj*,float,const Method*);
static ShowChoice originalChoice;
static void showChoice(Obj* self,Obj* callback,Obj* question,Obj* answers,Obj* ids,float timeout,const Method* m) {
    navigation::cancel("Automatic walking stopped for the question.",false);
    originalChoice(self,callback,question,answers,ids,timeout,m);clearChoice();stopSpeech();
    auto localise=[&](Obj* key){return localized(self,key);};
    std::vector<std::string> spokenAnswers;for(auto answer:array(answers))spokenAnswers.push_back(localise(answer));
    if(callback&&choices.set(localise(question),spokenAnswers,strings(ids))){callbackHandle=gchandle_new(callback,false);questionOwnerHandle=gchandle_new(self,false);log("QUESTION timeout="+std::to_string(timeout)+" "+choices.all());say(choices.all()+(pauseChoiceTimers?" Choice timer paused.":""));syncChoiceFocus(true);}
    else log("ERROR question data invalid: no callback or mismatched answer arrays");
}
using Simple=void(*)(Obj*,const Method*);static Simple originalHide,originalEventUpdate,originalDocHide;
static void docHide(Obj* self,const Method* m){originalDocHide(self,m);if(lastContent==lastDocument)lastContent=lastSubtitle;lastDocument.clear();std::lock_guard<std::mutex> lock(speechMutex);if(documentEpoch&&documentEpoch==speechEpoch){++speechEpoch;speechQueue.clear();speechQueue.push_back({"",true,speechEpoch});}documentEpoch=0;}
static void hideChoices(Obj* self,const Method* m){clearChoice();stopSpeech();originalHide(self,m);}
static void eventUpdate(Obj* self,const Method* m){originalEventUpdate(self,m);pump();}
using Update=void(*)(Obj*,float,const Method*);static Update originalWorldUpdate;
static void worldUpdate(Obj* self,float dt,const Method* m){if(!currentWorldUiHandle||gchandle_get_target(currentWorldUiHandle)!=self){if(currentWorldUiHandle)gchandle_free(currentWorldUiHandle);currentWorldUiHandle=gchandle_new(self,false);}originalWorldUpdate(self,dt,m);pump();navigation::tick(dt);readWorldHints(self);}
static Update originalDialogueUpdate;
static void dialogueUpdate(Obj* self,float dt,const Method* m){originalDialogueUpdate(self,pauseChoiceTimers&&choices.active?0.0f:dt,m);}
using Subtitle=void(*)(Obj*,Obj*,Obj*,const Method*);static Subtitle originalSubtitle;
static void subtitle(Obj* self,Obj* character,Obj* str,const Method* m){originalSubtitle(self,character,str,m);auto content=localized(self,str);if(character){auto speaker=localized(self,invoke(character,"get_localisedName"));if(!speaker.empty())content=speaker+". "+content;}if(content!=lastSubtitle){lastSubtitle=content;lastContent=content;if(readSubtitles&&!choices.active)say(content);}}
// Unity Vector2 is passed as an eight-byte aggregate on Windows x64.
using TryShow=void(*)(Obj*,Obj*,uint64_t,const Method*);static TryShow originalDocument;
static void document(Obj* self,Obj* value,uint64_t size,const Method* m){originalDocument(self,value,size,m);auto component=field(self,"_textComponent");auto content=componentText(component);if(!content.empty()&&content!=lastDocument){lastDocument=content;lastContent=content;say(content);std::lock_guard<std::mutex> lock(speechMutex);documentEpoch=muted?0:speechEpoch;}}
static bool hook(Class* c,const char* name,int count,void* replacement,void** original) {
    auto m=method(c,name,count);if(!m){log(std::string("Missing hook ")+name);return false;}auto address=*reinterpret_cast<void*const*>(m);if(!address)return false;
    auto result=MH_CreateHook(address,replacement,original);if(result!=MH_OK){log(std::string("Hook failure ")+name+": "+MH_StatusToString(result));return false;}
    log(std::string("Hook ready ")+name);return true;
}
static DWORD WINAPI startup(void*) {
    void* attached=nullptr;
    try{
        log("Killer Frequency accessibility starting");HANDLE worker=CreateThread(nullptr,0,speechWorker,nullptr,0,nullptr);if(worker)CloseHandle(worker);
        HANDLE watchdog=CreateThread(nullptr,0,navigation::releaseWatchdog,nullptr,0,nullptr);if(watchdog){navigation::watchdogReady=true;CloseHandle(watchdog);}else log("Door assistance disabled: input release watchdog could not start");
        for(int i=0;i<1200;++i){game=GetModuleHandleW(L"GameAssembly.dll");if(game)break;Sleep(100);}if(!game)throw std::runtime_error("GameAssembly did not load");bindAPI();
        for(int i=0;i<1200&&!domain_get();++i)Sleep(100);
        if(!domain_get())throw std::runtime_error("IL2CPP domain did not initialize");
        // Register before any API that may allocate or trigger garbage collection.
        attached=thread_attach(domain_get());
        log("IL2CPP worker attached");
        for(int i=0;i<1200;++i){size_t n=0;domain_get_assemblies(domain_get(),&n);if(n>10)break;Sleep(100);}
        auto world=cls("Assembly-CSharp.dll","Services","WorldUIManager");eventClass=cls("UnityEngine.UI.dll","UnityEngine.EventSystems","EventSystem");tmpClass=cls("Unity.TextMeshPro.dll","TMPro","TMP_Text");textClass=cls("UnityEngine.UI.dll","UnityEngine.UI","Text");selectableClass=cls("UnityEngine.UI.dll","UnityEngine.UI","Selectable");objectClass=cls("UnityEngine.CoreModule.dll","UnityEngine","Object");
        if(!world||!eventClass||!objectClass||!selectableClass)throw std::runtime_error("Required game types not found");
        if(MH_Initialize()!=MH_OK)throw std::runtime_error("MinHook initialization failed");
        bool ok=hook(world,"ShowDialogueChoice",5,(void*)showChoice,(void**)&originalChoice);
        ok=hook(world,"HideDialogAnswers",0,(void*)hideChoices,(void**)&originalHide)&&ok;
        ok=hook(eventClass,"Update",0,(void*)eventUpdate,(void**)&originalEventUpdate)&&ok;
        ok=hook(world,"OnUpdate",1,(void*)worldUpdate,(void**)&originalWorldUpdate)&&ok;
        ok=hook(cls("Assembly-CSharp.dll","Services","DialogueManager"),"OnUpdate",1,(void*)dialogueUpdate,(void**)&originalDialogueUpdate)&&ok;
        hook(world,"ShowSubtitle",2,(void*)subtitle,(void**)&originalSubtitle);
        hook(cls("Assembly-CSharp.dll","","DiegeticTextDisplay"),"TryShow",2,(void*)document,(void**)&originalDocument);
        hook(cls("Assembly-CSharp.dll","","DiegeticTextDisplay"),"Hide",0,(void*)docHide,(void**)&originalDocHide);
        if(!ok){MH_Uninitialize();throw std::runtime_error("Required hooks failed; no hooks enabled");}
        if(MH_EnableHook(MH_ALL_HOOKS)!=MH_OK)throw std::runtime_error("Hook activation failed");
        log("READY: dialogue and menu hooks active");say("Killer Frequency accessibility ready. Press F1 for controls.");
    }catch(const std::exception& e){log(std::string("ERROR ")+e.what());say("Killer Frequency accessibility could not initialize. Check the log.");}
    if(attached)thread_detach(attached);
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);wchar_t path[MAX_PATH];GetModuleFileNameW(module,path,MAX_PATH);base=fs::path(path).parent_path();auto t=CreateThread(nullptr,0,startup,nullptr,0,nullptr);if(t)CloseHandle(t);}return TRUE;
}
