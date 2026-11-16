// Only caller-selected diagnostic actions execute. Managed calls run on Unity's main thread.
const game = Process.getModuleByName('GameAssembly.dll');
const api = (name, result, args) => new NativeFunction(game.getExportByName('il2cpp_' + name), result, args);
const domain = api('domain_get', 'pointer', []);
const attach = api('thread_attach', 'pointer', ['pointer']);
const detach = api('thread_detach', 'void', ['pointer']);
const assemblies = api('domain_get_assemblies', 'pointer', ['pointer', 'pointer']);
const image = api('assembly_get_image', 'pointer', ['pointer']);
const imageName = api('image_get_name', 'pointer', ['pointer']);
const findClass = api('class_from_name', 'pointer', ['pointer', 'pointer', 'pointer']);
const objectClass = api('object_get_class', 'pointer', ['pointer']);
const parent = api('class_get_parent', 'pointer', ['pointer']);
const methods = api('class_get_methods', 'pointer', ['pointer', 'pointer']);
const methodName = api('method_get_name', 'pointer', ['pointer']);
const paramCount = api('method_get_param_count', 'uint', ['pointer']);
const methodGeneric = api('method_is_generic', 'bool', ['pointer']);
const paramType = api('method_get_param', 'pointer', ['pointer', 'uint']);
const typeName = api('type_get_name', 'pointer', ['pointer']);
const free = api('free', 'void', ['pointer']);
const classType = api('class_get_type', 'pointer', ['pointer']);
const typeObject = api('type_get_object', 'pointer', ['pointer']);
const invoke = api('runtime_invoke', 'pointer', ['pointer', 'pointer', 'pointer', 'pointer']);
const chars = api('string_chars', 'pointer', ['pointer']);
const stringLength = api('string_length', 'int', ['pointer']);
const unbox = api('object_unbox', 'pointer', ['pointer']);
const fieldInfo = api('class_get_field_from_name', 'pointer', ['pointer', 'pointer']);
const fieldValue = api('field_get_value', 'void', ['pointer', 'pointer', 'pointer']);
const cstr = value => Memory.allocUtf8String(value);
function cls(assembly, ns, name) {
    const count = Memory.alloc(8); const list = assemblies(domain(), count);
    for (let i=0; i<count.readU64().toNumber(); i++) {
        const im=image(list.add(i*8).readPointer());
        if(imageName(im).readUtf8String()===assembly) return findClass(im,cstr(ns),cstr(name));
    } return ptr(0);
}
function method(c,name,count,firstType) {
    for(;!c.isNull();c=parent(c)) {
        const iterator=Memory.alloc(8); iterator.writePointer(ptr(0));
        for(;;) { const m=methods(c,iterator); if(m.isNull())break;
            if(methodName(m).readUtf8String()!==name||paramCount(m)!==count||methodGeneric(m))continue;
            if(firstType) {const t=typeName(paramType(m,0));const s=t.readUtf8String();free(t);if(s!==firstType)continue;}
            return m;
        }
    }return ptr(0);
}
function call(m,obj=ptr(0),args=[]) {
    if(m.isNull())return ptr(0);
    const a=Memory.alloc(Math.max(8,args.length*8));args.forEach((v,i)=>a.add(i*8).writePointer(v));
    const error=Memory.alloc(8);error.writePointer(ptr(0));const result=invoke(m,obj,args.length?a:ptr(0),error);
    if(!error.readPointer().isNull())throw Error('Managed exception from '+methodName(m).readUtf8String());return result;
}
function run(o,name,args=[],firstType) {return o.isNull()?ptr(0):call(method(objectClass(o),name,args.length,firstType),o,args);}
function field(o,name) {if(o.isNull())return ptr(0);for(let c=objectClass(o);!c.isNull();c=parent(c)){const f=fieldInfo(c,cstr(name));if(!f.isNull()){const out=Memory.alloc(8);fieldValue(o,f,out);return out.readPointer();}}return ptr(0);}
function str(o){return o.isNull()?null:chars(o).readUtf16String(stringLength(o));}
function arr(o){if(o.isNull())return [];const n=o.add(24).readU64().toNumber();if(n>30000)throw Error('Array too large');return Array.from({length:n},(_,i)=>o.add(32+i*8).readPointer());}
function find(c){return arr(call(method(cls('UnityEngine.CoreModule.dll','UnityEngine','Object'),'FindObjectsOfType',1,'System.Type'),ptr(0),[typeObject(classType(c))]));}
function vec(o){if(o.isNull())return null;const p=unbox(o);return [p.readFloat(),p.add(4).readFloat(),p.add(8).readFloat()];}
function scene() {
    const transforms=find(cls('UnityEngine.CoreModule.dll','UnityEngine','Transform'));
    return transforms.map(t=>({name:str(run(t,'get_name')),parent:str(run(run(t,'get_parent'),'get_name')),position:vec(run(t,'get_position')),active:unbox(run(run(t,'get_gameObject'),'get_activeInHierarchy')).readU8()}));
}
function typed(assembly,ns,name) {return find(cls(assembly,ns,name)).map(o=>({name:str(run(o,'get_name')),position:vec(run(run(o,'get_transform'),'get_position'))}));}
let pending=null;
const registration=attach(domain());
const tick=method(cls('UnityEngine.UI.dll','UnityEngine.EventSystems','EventSystem'),'Update',0).readPointer();
detach(registration);
const listener=Interceptor.attach(tick,{onEnter(){if(!pending)return;const action=pending;pending=null;try{
    if(action==='scene')send({scene:scene(),players:typed('Assembly-CSharp.dll','','FlatScreenPlayer'),doors:typed('Assembly-CSharp.dll','','DoorOpenable'),targets:typed('Assembly-CSharp.dll','Player.Movement','MoveTarget')});
    else throw Error('Unknown diagnostic action '+action);
}catch(e){send({error:String(e),stack:e.stack});}}});
rpc.exports={schedule(action){pending=action;}};
