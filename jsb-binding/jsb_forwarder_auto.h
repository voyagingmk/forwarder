//
//  jsb_forwarder_auto.h
//  tiancai-test
//
//  Created by 邱嘉伟 on 2016/11/13.
//
//

#ifndef jsb_forwarder_auto_h
#define jsb_forwarder_auto_h

#include "jsapi.h"
#include "jsfriendapi.h"
#if (COCOS2D_VERSION >= 0x00031100)
#include "scripting/js-bindings/manual/js_manual_conversions.h"
#include "scripting/js-bindings/manual/cocos2d_specifics.hpp"
#include "scripting/js-bindings/manual/ScriptingCore.h"
#else
#include "js_manual_conversions.h"
#include "cocos2d_specifics.hpp"
#include "ScriptingCore.h"
#endif

void  register_all_forwarder(JSContext* cx, JS::HandleObject obj);

#endif /* jsb_forwarder_auto_h */
