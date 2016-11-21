//
//  jsb_forwarder_auto.cpp
//  tiancai-test
//
//  Created by 邱嘉伟 on 2016/11/13.
//
//

#include <stdio.h>
#include "cocos2d.h"
#include "forwardctrl.h"
#include "jsapi.h"
#include "jsb_forwarder_auto.h"


//前置声明
void  js_register_ForwardCtrl(JSContext* cx, JS::HandleObject global);

// 定义 js 端的类型
JSClass  *jsb_ForwardCtrl_class;
JSObject *jsb_ForwardCtrl_prototype;

// 实现 ls 命名空间下的类绑定
void register_all_forwarder(JSContext* cx, JS::HandleObject obj) {
    JS::RootedObject ns(cx);
    get_or_create_js_obj(cx, obj, "forwarder", &ns);
    
    js_register_ForwardCtrl(cx, ns);
}

void js_cocos2d_ForwardCtrl_finalize(JSFreeOp *fop, JSObject *obj) {
    CCLOGINFO("jsbindings: finalizing ForwardCtrl object %p (Node)", obj);
}

static bool js_is_native_obj(JSContext *cx, uint32_t argc, jsval *vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    args.rval().setBoolean(true);
    return true;
}



///////////////////////////////////////////////////
///////////////////////////////////////////////////

bool js_cocos2d_ForwardCtrl_create(JSContext *cx, uint32_t argc, jsval *vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    if (argc == 0) {
        forwarder::ForwardCtrl* ret = new forwarder::ForwardCtrl();
        jsval jsret = JSVAL_NULL;
        do {
            if (ret) {
#if (COCOS2D_VERSION >= 0x00031100)
                jsret = OBJECT_TO_JSVAL(js_get_or_create_jsobject<forwarder::ForwardCtrl>(cx, (forwarder::ForwardCtrl*)ret));
#else
                js_proxy_t *jsProxy = js_get_or_create_proxy<forwarder::ForwardCtrl>(cx, ret);
                jsret = OBJECT_TO_JSVAL(jsProxy->obj);
#endif
            } else {
                jsret = JSVAL_NULL;
            }
        } while (0);
        args.rval().set(jsret);
        return true;
    }
    JS_ReportError(cx, "js_cocos2dx_EnetMgr_create : wrong number of arguments");
    return false;
}


bool js_cocos2dx_ForwardCtrl_constructor(JSContext *cx, uint32_t argc, jsval *vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    bool ok = true;
    forwarder::ForwardCtrl* cobj = new (std::nothrow) forwarder::ForwardCtrl();
    cocos2d::Ref *_ccobj = dynamic_cast<cocos2d::Ref *>(cobj);
    if (_ccobj) {
        _ccobj->autorelease();
    }
    TypeTest<forwarder::ForwardCtrl> t;
    js_type_class_t *typeClass = nullptr;
    std::string typeName = t.s_name();
    auto typeMapIter = _js_global_type_map.find(typeName);
    CCASSERT(typeMapIter != _js_global_type_map.end(), "Can't find the class type!");
    typeClass = typeMapIter->second;
    CCASSERT(typeClass, "The value is null.");
    // JSObject *obj = JS_NewObject(cx, typeClass->jsclass, typeClass->proto, typeClass->parentProto);
    JS::RootedObject proto(cx, typeClass->proto.ref());
    JS::RootedObject parent(cx, typeClass->parentProto.ref());
    JS::RootedObject obj(cx, JS_NewObject(cx, typeClass->jsclass, proto, parent));
    args.rval().set(OBJECT_TO_JSVAL(obj));
    // link the native object with the javascript object
    js_proxy_t* p = jsb_new_proxy(cobj, obj);
    AddNamedObjectRoot(cx, &p->obj, "forwarder::ForwardCtrl");
    if (JS_HasProperty(cx, obj, "_ctor", &ok) && ok)
        ScriptingCore::getInstance()->executeFunctionWithOwner(OBJECT_TO_JSVAL(obj), "_ctor", args);
    return true;
}

///////////////////////////////////////////////////
///////////////////////////////////////////////////
#define CALL_MEM_FUNC_BEGIN(name, num) \
bool js_cocos2dx_ForwardCtrl_##name(JSContext *cx, uint32_t argc, jsval *vp) \
{  \
    int expectedArgcNum = num; \
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp); \
    JS::RootedObject obj(cx, args.thisv().toObjectOrNull()); \
    js_proxy_t *proxy = jsb_get_js_proxy(obj); \
    forwarder::ForwardCtrl* ctrl = (forwarder::ForwardCtrl *)(proxy ? proxy->ptr : NULL); \
    JSB_PRECONDITION2( ctrl, cx, false, "js_cocos2dx_EnetMgr_%s : Invalid Native Object", #name); \
    if (argc == expectedArgcNum) {

#define CALL_MEM_FUNC_END(name) \
    return true; \
    }\
    JS_ReportError(cx, "js_cocos2dx_EnetMgr_%s : wrong number of arguments: %d, was expecting %d", #name, argc, expectedArgcNum); \
    return false; \
    }




CALL_MEM_FUNC_BEGIN(setDebug, 1)
    int enabled;
    JS::ToInt32(cx, args.get(0), &enabled);
    ctrl->setDebug(enabled);
CALL_MEM_FUNC_END(setDebug)



CALL_MEM_FUNC_BEGIN(initProtocolMap, 1)
    const char* data;
    JSString* jsValue = JS::ToString( cx, args.get(0));
    JSStringWrapper w(jsValue);
    data = w.get();
    rapidjson::Document config;
    config.Parse(data);
    int ret = ctrl->initProtocolMap(config);
    args.rval().set(INT_TO_JSVAL(ret));
CALL_MEM_FUNC_END(initProtocolMap)


CALL_MEM_FUNC_BEGIN(createServer, 1)
    const char* data;
    JSString* jsValue = JS::ToString( cx, args.get(0));
    JSStringWrapper w(jsValue);
    data = w.get();
    rapidjson::Document config;
    config.Parse(data);
    int serverId = ctrl->createServer(config);
    args.rval().set(INT_TO_JSVAL(serverId));
CALL_MEM_FUNC_END(createServer)

CALL_MEM_FUNC_BEGIN(isConnected, 1)
    int serverId;
    JS::ToInt32(cx, args.get(0), &serverId);
    forwarder::ForwardServer* server = ctrl->getServerByID(serverId);
    bool connected;
    if(server) {
        connected = server->isConnected();
    }
    args.rval().set(BOOLEAN_TO_JSVAL(connected));
CALL_MEM_FUNC_END(isConnected)

CALL_MEM_FUNC_BEGIN(doDisconnect, 1)
    int serverId;
    JS::ToInt32(cx, args.get(0), &serverId);
    forwarder::ForwardServer* server = ctrl->getServerByID(serverId);
    if(server) {
        server->doDisconnect();
    }
CALL_MEM_FUNC_END(doDisconnect)


CALL_MEM_FUNC_BEGIN(pollOnce, 1)
    int serverId;
    JS::ToInt32(cx, args.get(0), &serverId);
    ctrl->pollOnceByServerID(serverId);
CALL_MEM_FUNC_END(pollOnce)


CALL_MEM_FUNC_BEGIN(getCurEvent, 0)
    forwarder::Event event = ctrl->getCurEvent();
    args.rval().set(INT_TO_JSVAL(event));
CALL_MEM_FUNC_END(getCurEvent)


CALL_MEM_FUNC_BEGIN(sendText, 2)
    int serverId;
    JS::ToInt32(cx, args.get(0), &serverId);
    const char* data;
    JSString* jsValue = JS::ToString( cx, args.get(1));
    JSStringWrapper w(jsValue);
    data = w.get();
    int ret = ctrl->sendText(serverId, data);
    args.rval().set(INT_TO_JSVAL(ret));
CALL_MEM_FUNC_END(sendText)



CALL_MEM_FUNC_BEGIN(sendBinary, 3)
    bool ok = true;
    int serverId;
    ok &= JS::ToInt32( cx, args.get(0), &serverId);
    int len;
    void * data;
    JSB_get_arraybufferview_dataptr(cx, args.get(0), &len, &data);
    int ret = ctrl->sendBinary(serverId, (uint8_t*)data, len);
    args.rval().set(INT_TO_JSVAL(ret));
CALL_MEM_FUNC_END(sendBinary)


CALL_MEM_FUNC_BEGIN(getCurPacketData, 0)
    forwarder::ForwardPacketPtr packet = ctrl->getCurProcessPacket();
    JSString* str = JS_NewStringCopyN(cx, (char*)packet->getDataPtr(), packet->getDataLength());
    args.rval().set(STRING_TO_JSVAL(str));
CALL_MEM_FUNC_END(getCurPacketData)




///////////////////////////////////////////////////
///////////////////////////////////////////////////

void js_register_ForwardCtrl(JSContext* cx, JS::HandleObject global){
    jsb_ForwardCtrl_class = (JSClass *)calloc(1, sizeof(JSClass));
    jsb_ForwardCtrl_class->name = "ForwardCtrl";
    jsb_ForwardCtrl_class->addProperty = JS_PropertyStub;
    jsb_ForwardCtrl_class->delProperty = JS_DeletePropertyStub;
    jsb_ForwardCtrl_class->getProperty = JS_PropertyStub;
    jsb_ForwardCtrl_class->setProperty = JS_StrictPropertyStub;
    jsb_ForwardCtrl_class->enumerate = JS_EnumerateStub;
    jsb_ForwardCtrl_class->resolve = JS_ResolveStub;
    jsb_ForwardCtrl_class->convert = JS_ConvertStub;
    jsb_ForwardCtrl_class->finalize = js_cocos2d_ForwardCtrl_finalize;
    jsb_ForwardCtrl_class->flags = JSCLASS_HAS_RESERVED_SLOTS(2);
    
    static JSPropertySpec properties[] = {
        JS_PSG("__nativeObj", js_is_native_obj, JSPROP_PERMANENT | JSPROP_ENUMERATE),
        JS_PS_END
    };
    
#define decl_func(name) JS_FN(#name, js_cocos2dx_ForwardCtrl_##name, 0, JSPROP_PERMANENT | JSPROP_ENUMERATE)
    
    static JSFunctionSpec funcs[] = {
        decl_func(setDebug),
        
        decl_func(initProtocolMap),
        
        decl_func(createServer),
        
        decl_func(isConnected),
        
        decl_func(doDisconnect),

        decl_func(getCurEvent),

        decl_func(pollOnce),

        decl_func(sendText),
        
        decl_func(sendBinary),
        
        decl_func(getCurPacketData),

        JS_FS_END
    };
#undef decl_func
    
    static JSFunctionSpec st_funcs[] = {
        JS_FN("create", js_cocos2d_ForwardCtrl_create, 0, JSPROP_PERMANENT | JSPROP_ENUMERATE),
        JS_FS_END
    };
    
    jsb_ForwardCtrl_prototype = JS_InitClass(
                                         cx, global,
                                         JS::NullPtr(), // parent proto
                                         jsb_ForwardCtrl_class,
                                         js_cocos2dx_ForwardCtrl_constructor, 0, // constructor
                                         properties,
                                         funcs,
                                         NULL, // no static properties
                                         st_funcs);
    // make the class enumerable in the registered namespace
    //  bool found;
    //FIXME: Removed in Firefox v27
    //  JS_SetPropertyAttributes(cx, global, "Node", JSPROP_ENUMERATE | JSPROP_READONLY, &found);
    
    // add the proto and JSClass to the type->js info hash table
    JS::RootedObject proto(cx, jsb_ForwardCtrl_prototype);
    jsb_register_class<forwarder::ForwardCtrl>(cx, jsb_ForwardCtrl_class, proto, JS::NullPtr());
    /*
     TypeTest<forwarder::ForwardCtrl> t;
     js_type_class_t *p;
     std::string typeName = t.s_name();
     if (_js_global_type_map.find(typeName) == _js_global_type_map.end())
     {
     p = (js_type_class_t *)malloc(sizeof(js_type_class_t));
     p->jsclass = jsb_ForwardCtrl_class;
     p->proto = jsb_ForwardCtrl_prototype;
     p->parentProto = NULL;
     _js_global_type_map.insert(std::make_pair(typeName, p));
     }
     */
}
