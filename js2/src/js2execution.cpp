/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */


#ifdef _WIN32
 // Turn off warnings about identifiers too long in browser information
#pragma warning(disable: 4786)
#pragma warning(disable: 4711)
#pragma warning(disable: 4710)
#endif

#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "parser.h"
#include "numerics.h"
#include "js2runtime.h"
#include "bytecodegen.h"

#include "jsstring.h"
#include "jsarray.h"
#include "jsmath.h"
#include "hash.h"

#include "fdlibm_ns.h"

// this is the AttributeList passed to the name lookup routines
#define CURRENT_ATTR    (NULL)

namespace JavaScript {    
namespace JS2Runtime {




inline char narrow(char16 ch) { return char(ch); }

JSValue Context::readEvalFile(const String& fileName)
{
    String buffer;
    int ch;

    JSValue result = kUndefinedValue;

    std::string str(fileName.length(), char());
    std::transform(fileName.begin(), fileName.end(), str.begin(), narrow);
    FILE* f = fopen(str.c_str(), "r");
    if (f) {
        while ((ch = getc(f)) != EOF)
            buffer += static_cast<char>(ch);
        fclose(f);
    
        Arena a;
        Parser p(mWorld, a, mFlags, buffer, fileName);
        mReader = &p.lexer.reader;
        StmtNode *parsedStatements = p.parseProgram();
        ASSERT(p.lexer.peek(true).hasKind(Token::end));
        if (mDebugFlag)
        {
            PrettyPrinter f(stdOut, 30);
            {
                PrettyPrinter::Block b(f, 2);
                f << "Program =";
                f.linearBreak(1);
                StmtNode::printStatements(f, parsedStatements);
            }
            f.end();
            stdOut << '\n';
        }

        buildRuntime(parsedStatements);
        JS2Runtime::ByteCodeModule* bcm = genCode(parsedStatements, fileName);
        if (bcm) {
            setReader(NULL);
            bcm->setSource(buffer, fileName);
            result = interpret(bcm, 0, NULL, JSValue(getGlobalObject()), NULL, 0);
            delete bcm;
        }
    }
    return result;
}

// Given an operator op, and two operand types - dispatch to the
// appropriate operator. The operands are still on the execution stack.
// Return result indicates whether interpreter loop has to begin
// execution of new function.
bool Context::executeOperator(Operator op, JSType *t1, JSType *t2)
{
    // look in the operator table for applicable operators
    OperatorList applicableOperators;

    for (OperatorList::iterator oi = mOperatorTable[op].begin(),
                end = mOperatorTable[op].end();
                    (oi != end); oi++) 
    {
        if ((*oi)->isApplicable(t1, t2)) {
            applicableOperators.push_back(*oi);
        }
    }
    if (applicableOperators.size() == 0)
        reportError(Exception::runtimeError, "No applicable operators found");

    OperatorList::iterator candidate = applicableOperators.begin();
    for (OperatorList::iterator aoi = applicableOperators.begin() + 1,
                aend = applicableOperators.end();
                (aoi != aend); aoi++) 
    {
        if ((*aoi)->mType1->derivesFrom((*candidate)->mType1)
                || ((*aoi)->mType2->derivesFrom((*candidate)->mType2)))
            candidate = aoi;
    }

    JSFunction *target = (*candidate)->mImp;

    JSValue newThis = kNullValue;
    if (target->isNative()) {
        JSValue result = target->getNativeCode()(this, newThis, getBase(stackSize() - 2), 2);
        resizeStack(stackSize() - 2);
        pushValue(result);
        return false;
    }
    else {
        mActivationStack.push(new Activation(mLocals, mStack, mStackTop - 2, mScopeChain,
                                                mArgumentBase, mThis, mPC, mCurModule));
        mThis = newThis;
        mCurModule = target->getByteCode();
        mArgumentBase = getBase(stackSize() - 2);
        mScopeChain = target->getScopeChain();
        return true;
    }
}


// Invokes either the native or bytecode implementation. Causes another interpreter loop
// to begin execution, and does nothing to clean up the incoming arguments (which need
// not even be on the execution stack).
JSValue Context::invokeFunction(JSFunction *target, const JSValue& thisValue, JSValue *argv, uint32 argc)
{
    if (target->isNative())
        return target->getNativeCode()(this, thisValue, argv, argc);
    else
        return interpret(target->getByteCode(), 0, target->getScopeChain(), thisValue, argv, argc);
}

JSValue Context::interpret(JS2Runtime::ByteCodeModule *bcm, int offset, ScopeChain *scopeChain, const JSValue& thisValue, JSValue *argv, uint32 /*argc*/)
{ 
    mActivationStack.push(new Activation(mLocals, mStack, mStackTop, mScopeChain,
                                            mArgumentBase, mThis, NULL, mCurModule));   // use NULL pc value to force interpret loop to exit
    mThis = thisValue;
    if (scopeChain)
        mScopeChain = scopeChain;
    else {
        mScopeChain = new ScopeChain(this, mWorld);
        mScopeChain->addScope(getGlobalObject());
    }
    if (mThis.isObject())
        mScopeChain->addScope(mThis.object);
//    mScopeChain->addScope(mActivationStack.top());
    mCurModule = bcm;
    uint8 *pc = mCurModule->mCodeBase + offset;
    uint8 *endPC = mCurModule->mCodeBase + mCurModule->mLength;
    mArgumentBase = argv;
    mLocals = new JSValue[mCurModule->mLocalsCount];
    mStack = new JSValue[mCurModule->mStackDepth];
    mStackMax = mCurModule->mStackDepth;
    mStackTop = 0;

    JSValue result = interpret(pc, endPC);

    Activation *prev = mActivationStack.top();
    mActivationStack.pop();

    mCurModule = prev->mModule;
    mStack = prev->mStack;
    mStackTop = prev->mStackTop;
    if (mCurModule)
        mStackMax = mCurModule->mStackDepth;
    mLocals = prev->mLocals;
    mArgumentBase = prev->mArgumentBase;
    mThis = prev->mThis;
    mScopeChain = prev->mScopeChain;

    return result;
}

// Assumes arguments are on the top of stack.
JSValue *Context::buildArgumentBlock(JSFunction *target, uint32 &argCount)
{
    JSValue *argBase;
    uint32 maxExpectedArgCount = target->getRequiredArgumentCount() + target->getOptionalArgumentCount();
    if (target->isChecked()) {
        if (argCount < target->getRequiredArgumentCount())
            reportError(Exception::referenceError, "Insufficient quantity of arguments");
    
        if ((argCount > maxExpectedArgCount) && !target->hasRestParameter())
            reportError(Exception::referenceError, "Oversufficient quantity of arguments");
    }

    uint32 i;
    uint32 argBlockSize = maxExpectedArgCount + (target->hasRestParameter() ? 1 : 0);
                                                    // room for all required & optional arguments
                                                    // plus the rest parameter if it exists.
    argBase = new JSValue[argBlockSize];     

    bool *argUsed = new bool[argCount];
    for (i = 0; i < argCount; i++)
        argUsed[i] = false;
    uint32 argStart = stackSize() - argCount;
    uint32 argIndex = 0;      // the index into argBase, the resolved outgoing arguments
    uint32 posArgIndex = 0;   // next positional arg from the incoming set
    for (i = 0; i < argCount; i++) {    // find the first positional (i.e. non-named arg)
        JSValue v = getValue(i + argStart);
        if (!v.isObject() || (v.object->mType != NamedArgument_Type)) {
            posArgIndex = i;
            break;
        }
    }
    // for each parameter - see if there's a named arg that matches
    // otherwise take the next non-named argument. (unless the parameter
    // is an optional one)
    while (argIndex < maxExpectedArgCount) {                        
        bool foundNamedArg = false;
        // find a matching named argument
        for (uint32 i = 0; i < argCount; i++) {
            JSValue v = getValue(i + argStart);
            if (v.isObject() && (v.object->mType == NamedArgument_Type)) {
                NamedArgument *arg = static_cast<NamedArgument *>(v.object);
                if (target->findParameterName(arg->mName) == argIndex) {
                    // mark this arg has having been used
                    argUsed[i] = true;
                    foundNamedArg = true;
                    argBase[argIndex] = arg->mValue;
                    break;
                }
            }
        }
        if (!foundNamedArg) {
            if (target->argHasInitializer(argIndex)) {
                argBase[argIndex] = target->runArgInitializer(this, argIndex, mThis, argBase, maxExpectedArgCount);
            }
            else {
                if (posArgIndex < argCount) {
                    argUsed[posArgIndex] = true; 
                    argBase[argIndex] = getValue(posArgIndex++ + argStart);
                }
                else {
                    if (target->isChecked())
                        reportError(Exception::referenceError, "Missing positional argument");
                    else
                        argBase[argIndex] = kUndefinedValue;
                }
            }
        }
        argIndex++;
    }

    JSValue restArgument;
    if (target->hasRestParameter() && target->getRestParameterName()) {
        restArgument = target->getRestParameterType()->newInstance(this);
        argBase[maxExpectedArgCount] = JSValue(restArgument);
    }
    posArgIndex = 0;    // re-number the non-named arguments that end up in the rest arg
    // now find a place for any left-overs
    for (i = 0; i < argCount; i++) {
        if (!argUsed[i]) {
            JSValue v = getValue(i + argStart);
            if (v.isObject() && (v.object->mType == NamedArgument_Type)) {
                NamedArgument *arg = static_cast<NamedArgument *>(v.object);
                // if this argument matches a parameter name, that's bad because
                // it's a duplicate case
                if (target->findParameterName(arg->mName) != NotABanana)
                    reportError(Exception::referenceError, "Duplicate named argument");                                
                else {
                    if (target->hasRestParameter()) {
                        if (!restArgument.isUndefined())
                            // XXX is it an error to have duplicate named rest properties?
                            restArgument.object->setProperty(this, *arg->mName, (NamespaceList *)(NULL), arg->mValue);
                    }
                    else
                        reportError(Exception::referenceError, "Unknown named argument, no rest argument");                                
                }
            }
            else {
                if (target->hasRestParameter()) {
                    if (!restArgument.isUndefined()) {
                        String *id = numberToString(posArgIndex++);
                        restArgument.object->setProperty(this, *id, (NamespaceList *)(NULL), v);
                    }
                }
                else
                    reportError(Exception::referenceError, "Extra argument, no rest argument");                                
            }
        }
    }
    argCount = argBlockSize;
    return argBase;
}

JSValue Context::interpret(uint8 *pc, uint8 *endPC)
{
    JSValue result = kUndefinedValue;
    while (pc != endPC) {
        mPC = pc;
        try {
            if (mDebugFlag) {
                if (mCurModule->mFunction) {
                    FunctionName *fnName = mCurModule->mFunction->getFunctionName();
                    StringFormatter s;
                    PrettyPrinter pp(s);
                    fnName->print(pp);
                    const String &fnStr = s.getString();
                    std::string str(fnStr.length(), char());
                    std::transform(fnStr.begin(), fnStr.end(), str.begin(), narrow);
                    int len = strlen(str.c_str());
                    printFormat(stdOut, "%.30s+%.4d%*c%d        ", str.c_str(), (pc - mCurModule->mCodeBase), (len > 30) ? 0 : (len - 30), ' ', stackSize());
                }
                else
                    printFormat(stdOut, "+%.4d%*c%d        ", (pc - mCurModule->mCodeBase), 30, ' ', stackSize());
                printInstruction(stdOut, toUInt32(pc - mCurModule->mCodeBase), *mCurModule);
            }
            switch ((ByteCodeOp)(*pc++)) {
            case PopOp:
                {
                    result = popValue(); // XXX debug only? - just decrement top
                }
                break;
            case DupOp:
                {
                    JSValue v = topValue();
                    pushValue(v);
                }
                break;
            case DupInsertOp:   // XXX something more efficient than pop/push?
                {
                    JSValue v1 = popValue();
                    JSValue v2 = popValue();
                    pushValue(v1);
                    pushValue(v2);
                    pushValue(v1);
                }
                break;
            case DupNOp:
                {
                    JSValue v = topValue();
                    uint16 count = *((uint16 *)pc);
                    pc += sizeof(uint16);
                    while (count--)
                        pushValue(v);
                }
                break;
            // <object> {xN} <object2> --> <object2> <object> {xN} <object2>
            case DupInsertNOp:
                {
                    JSValue v2 = topValue();
                    uint16 count = *((uint16 *)pc);
                    pc += sizeof(uint16);
                    insertValue(v2, mStackTop - (count + 1));
                }
                break;
            case SwapOp:   // XXX something more efficient than pop/push?
                {
                    JSValue v1 = popValue();
                    JSValue v2 = popValue();
                    pushValue(v1);
                    pushValue(v2);
                }
                break;
            case LogicalXorOp:
                {
                    JSValue v2 = popValue();
                    ASSERT(v2.isBool());
                    JSValue v1 = popValue();
                    ASSERT(v1.isBool());

                    if (v1.boolean) {
                        if (v2.boolean) {
                            popValue();
                            popValue();
                            pushValue(kFalseValue);
                        }
                        else
                            popValue();
                    }
                    else {
                        if (v1.boolean) {
                            popValue();
                            popValue();
                            pushValue(kFalseValue);
                        }
                        else {
                            JSValue t = topValue();
                            popValue();
                            popValue();
                            pushValue(t);
                        }
                    }
                }
                break;
            case LogicalNotOp:
                {
                    JSValue v = popValue();
                    ASSERT(v.isBool());
                    pushValue(JSValue(!v.boolean));
                }
                break;
            case JumpOp:
                {
                    uint32 offset = *((uint32 *)pc);
                    pc += offset;
                }
                break;
            case ToBooleanOp:
                {
                    JSValue v = popValue();
                    pushValue(v.toBoolean(this));
                }
                break;
            case JumpFalseOp:
                {
                    JSValue v = popValue();
                    ASSERT(v.isBool());
                    if (!v.boolean) {
                        uint32 offset = *((uint32 *)pc);
                        pc += offset;
                    }
                    else
                        pc += sizeof(uint32);
                }
                break;
            case JumpTrueOp:
                {
                    JSValue v = popValue();
                    ASSERT(v.isBool());
                    if (v.boolean) {
                        uint32 offset = *((uint32 *)pc);
                        pc += offset;
                    }
                    else
                        pc += sizeof(uint32);
                }
                break;
            case NamedArgOp:
                {
                    JSValue name = popValue();
                    if (!name.isString())
                        reportError(Exception::typeError, "String needed for argument name");
                    JSValue value = popValue();
                    pushValue(JSValue(new NamedArgument(value, name.string)));
                }
                break;
            case InvokeOp:
                {
                    uint32 argCount = *((uint32 *)pc); 
                    uint32 cleanUp = argCount;
                    pc += sizeof(uint32);
                    CallFlag callFlags = (CallFlag)(*pc++);
                    mPC = pc;

                    JSValue *targetValue = getBase(stackSize() - (argCount + 1));
                    JSFunction *target;
                    JSValue oldThis = mThis;
                    switch (callFlags & ThisFlags) {
                    case NoThis:
                        mThis = kNullValue; 
                        break;
                    case Explicit:
                        mThis = getValue(stackSize() - (argCount + 2));
                        cleanUp++;
                        break;
                    default:
                        NOT_REACHED("bad bytecode");
                    }

                    if (!targetValue->isFunction()) {
                        if (targetValue->isType()) {
/*
                            // "  Type()  "
                            // - it's a cast expression, we call the
                            // default constructor, overriding the supplied 'this'.
                            //
                            
                            // XXX Note that this is different behaviour from JS1.5, where
                            // (e.g.) Array(2) is an invocation of the constructor.

                            if ((argCount != 0) || ((callFlags & ThisFlags) != NoThis))
                                reportError(Exception::referenceError, "Type cast can only take one argument");
                            JSValue v = popValue();
                            popValue();             // don't need the target anymore
                            pushValue(mapValueToType(v, targetValue->type));
                            break;      // all done
*/

                            // how to distinguish between a cast and an invocation of
                            // the superclass constructor from a constructor????

                            // XXX help
                            target = targetValue->type->getDefaultConstructor();

                            // in this case, calling the constructor requires passing the 'this' value
                            // through.
                            mThis = oldThis;
                        }
                        else
                            reportError(Exception::referenceError, "Not a function");
                    }
                    else {
                        target = targetValue->function;

                        // an invocation of the super constructor has to pass thru the existing 'this'
                        if (target->isConstructor() && (callFlags & SuperInvoke))
                            mThis = oldThis;
                        else
                            if (target->hasBoundThis())   // then we use it instead of the expressed version
                                mThis = target->getThisValue();
                    }
                    if (target->isPrototype() && mThis.isNull())
                        mThis = JSValue(getGlobalObject());
                                        
                    JSValue *argBase = getBase(stackSize() - argCount);

                    if (!target->isNative()) {

                        // XXX Optimize the more typical case in which there are no named arguments, no optional arguments
                        // and the appropriate number of arguments have been supplied.

                        argBase = buildArgumentBlock(target, argCount);

                        mActivationStack.push(new Activation(mLocals, mStack, mStackTop - (cleanUp + 1),
                                                                    mScopeChain,
                                                                    mArgumentBase, oldThis,
                                                                    pc, mCurModule));
                        mScopeChain = target->getScopeChain();
                        if (mThis.isObject())
                            mScopeChain->addScope(mThis.object);
//                        mScopeChain->addScope(mActivationStack.top());
                        mCurModule = target->getByteCode();
                        pc = mCurModule->mCodeBase;
                        endPC = mCurModule->mCodeBase + mCurModule->mLength;
                        mArgumentBase = argBase;
                        mLocals = new JSValue[mCurModule->mLocalsCount];
                        mStack = new JSValue[mCurModule->mStackDepth];
                        mStackMax = mCurModule->mStackDepth;
                        mStackTop = 0;
                    }
                    else {
                        JSValue result = (target->getNativeCode())(this, mThis, argBase, argCount);
                        mThis = oldThis;
                        resizeStack(stackSize() - (cleanUp + 1));
                        pushValue(result);
                    }

                }
                break;
            case ReturnVoidOp:
                {
                    if (mActivationStack.empty())
                        return result;
                    Activation *prev = mActivationStack.top();
                    if (prev->mPC == NULL)     // NULL is used to indicate that we want the loop to exit
                        return result;         // (even though there is more activation stack to go
                                               // - used to implement Xetters from XProperty ops. e.g.)
                    mActivationStack.pop();

                    mCurModule = prev->mModule;
                    pc = prev->mPC;
                    endPC = mCurModule->mCodeBase + mCurModule->mLength;
                    mStack = prev->mStack;
                    mStackTop = prev->mStackTop;
                    mStackMax = mCurModule->mStackDepth;
                    mLocals = prev->mLocals;
                    mArgumentBase = prev->mArgumentBase;
                    mThis = prev->mThis;
                    mScopeChain = prev->mScopeChain;                                        
                }
                break;
            case ReturnOp:
                {
                    JSValue result = popValue();

                    if (mActivationStack.empty())
                        return result;
                    Activation *prev = mActivationStack.top();
                    if (prev->mPC == NULL)
                        return result;

                    mActivationStack.pop();

                    mCurModule = prev->mModule;
                    pc = prev->mPC;
                    endPC = mCurModule->mCodeBase + mCurModule->mLength;
                    mStack = prev->mStack;
                    mStackTop = prev->mStackTop;
                    mStackMax = mCurModule->mStackDepth;
                    mLocals = prev->mLocals;
                    mArgumentBase = prev->mArgumentBase;
                    mThis = prev->mThis;
                    mScopeChain = prev->mScopeChain;
                    pushValue(result);
                }
                break;
            case LoadTypeOp:
                {
                    JSType *t = *((JSType **)pc);
                    pc += sizeof(JSType *);
                    pushValue(JSValue(t));
                }
                break;
            case LoadFunctionOp:
                {
                    JSFunction *f = *((JSFunction **)pc);
                    pc += sizeof(JSFunction *);
                    pushValue(JSValue(f));
                }
                break;
            case LoadConstantStringOp:
                {
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    pushValue(JSValue(mCurModule->getString(index)));
                }
                break;
            case LoadConstantNumberOp:
                {
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    pushValue(JSValue(mCurModule->getNumber(index)));
                }
                break;
            case LoadConstantUndefinedOp:
                pushValue(kUndefinedValue);
                break;
            case LoadConstantTrueOp:
                pushValue(kTrueValue);
                break;
            case LoadConstantFalseOp:
                pushValue(kFalseValue);
                break;
            case LoadConstantNullOp:
                pushValue(kNullValue);
                break;
            case LoadConstantZeroOp:
                pushValue(kPositiveZero);
                break;
            case DeleteOp:
                {
                    JSValue base = popValue();
                    JSObject *obj = NULL;
                    if (!base.isObject() && !base.isType())
                        obj = base.toObject(this).object;
                    else
                        obj = base.object;
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    const String &name = *mCurModule->getString(index);
                    PropertyIterator it;
                    if (obj->hasOwnProperty(name, CURRENT_ATTR, Read, &it))
                        obj->deleteProperty(name, CURRENT_ATTR);
                    pushValue(kTrueValue);
                }
                break;
            case TypeOfOp:
                {
                    JSValue v = popValue();
                    if (v.isUndefined())
                        pushValue(JSValue(new String(widenCString("undefined"))));
                    else
                    if (v.isNull())
                        pushValue(JSValue(new String(widenCString("object"))));
                    else
                    if (v.isBool())
                        pushValue(JSValue(new String(widenCString("boolean"))));
                    else
                    if (v.isNumber())
                        pushValue(JSValue(new String(widenCString("number"))));
                    else
                    if (v.isString())
                        pushValue(JSValue(new String(widenCString("string"))));
                    else
                    if (v.isFunction())
                        pushValue(JSValue(new String(widenCString("function"))));
                    else
                        pushValue(JSValue(new String(widenCString("object"))));
                }
                break;
            case AsOp:
                {
                    JSValue t = popValue();
                    JSValue v = popValue();
                    if (t.isType()) {
                        if (v.isObject() 
                                && (v.object->getType() == t.type))
                            pushValue(v);
                        else
                            pushValue(kNullValue);   // XXX or throw an exception if 
                                                            // NULL is not a member of type t
                    }
                    else
                        reportError(Exception::typeError, "As needs type");
                }
                break;
            case IsOp:
                {
                    JSValue t = popValue();
                    JSValue v = popValue();
                    if (t.isType()) {
                        if (v.isNull())
                            if (t.type == Object_Type)
                                pushValue(kTrueValue);
                            else
                                pushValue(kFalseValue);
                        else
                            if (v.isObject() 
                                    && ((v.object->getType() == t.type)
                                        || (v.object->getType()->derivesFrom(t.type))))
                                pushValue(kTrueValue);
                            else {
                                if (v.getType() == t.type)
                                    pushValue(kTrueValue);
                                else
                                    pushValue(kFalseValue);
                            }
                    }
                    else {  // behave like instanceof
                        if (t.isObject() && t.isFunction()) {                            
                            // XXX prove that t->function["prototype"] is on t.object->mPrototype chain
                            pushValue(kTrueValue);
                        }
                        else
                            reportError(Exception::typeError, "InstanceOf needs object");
                    }
                }
                break;
            case InstanceOfOp:  
                {
                    JSValue t = popValue();
                    JSValue v = popValue();
                    if (t.isFunction()) {
                        JSFunction *obj = t.function;                        
                        PropertyIterator i;
                        JSFunction *target = NULL;
                        if (obj->hasProperty(widenCString("hasInstance"), CURRENT_ATTR, Read, &i)) {
                            JSValue hi = obj->getPropertyValue(i);
                            if (hi.isFunction())
                                target = hi.function;
                        }
                        if (target)
                            pushValue(invokeFunction(target, t, &v, 1));
                        else
                            reportError(Exception::typeError, "InstanceOf couldn't find [[hasInstance]]");
                    }
                    else
                        reportError(Exception::typeError, "InstanceOf needs function object");
                }
                break;
            case GetNameOp:
                {
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    const String &name = *mCurModule->getString(index);
                    mScopeChain->getNameValue(this, name, CURRENT_ATTR);
                }
                break;
            case GetTypeOfNameOp:
                {
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    const String &name = *mCurModule->getString(index);
                    if (mScopeChain->hasNameValue(name, CURRENT_ATTR)) {
                        mScopeChain->getNameValue(this, name, CURRENT_ATTR);
                    }
                    else
                        pushValue(kUndefinedValue);
                }
                break;
            case SetNameOp:
                {
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    const String &name = *mCurModule->getString(index);
                    mScopeChain->setNameValue(this, name, CURRENT_ATTR);
                }
                break;
            case GetElementOp:
                {
                    uint32 dimCount = *((uint16 *)pc);
                    pc += sizeof(uint16);
                    mPC = pc;

                    JSValue *baseValue = getBase(stackSize() - (dimCount + 1));

                    // Use the type of the base to dispatch on...
                    JSObject *obj = NULL;
                    if (!baseValue->isObject() && !baseValue->isType())
                        obj = baseValue->toObject(this).object;
                    else
                        obj = baseValue->object;
                    JSFunction *target = obj->getType()->getUnaryOperator(Index);

                    if (target) {
                        JSValue result;
                        if (target->isNative())
                            result = target->getNativeCode()(this, kNullValue, baseValue, dimCount + 1);
                        else {
                            uint32 argCount = dimCount + 1;
                            JSValue *argBase = buildArgumentBlock(target, argCount);
                            result = interpret(target->getByteCode(), 0, target->getScopeChain(), kNullValue, argBase, argCount);
                        }
                        resizeStack(stackSize() - (dimCount + 1));
                        pushValue(result);
                    }
                    else {  // XXX or should this be implemented in Object_Type as operator "[]" ?
                        if (dimCount != 1)
                            reportError(Exception::typeError, "too many indices");
                        JSValue index = popValue();
                        popValue();     // discard base
                        const String *name = index.toString(this).string;
                        obj->getProperty(this, *name, CURRENT_ATTR);
                    }
                }
                break;
            case SetElementOp:
                {
                    uint32 dimCount = *((uint16 *)pc);
                    pc += sizeof(uint16);
                    mPC = pc;

                    JSValue *baseValue = getBase(stackSize() - (dimCount + 2));     // +1 for assigned value

                    // Use the type of the base to dispatch on...
                    JSObject *obj = NULL;
                    if (!baseValue->isObject() && !baseValue->isType())
                        obj = baseValue->toObject(this).object;
                    else
                        obj = baseValue->object;
                    JSFunction *target = obj->getType()->getUnaryOperator(IndexEqual);

                    if (target) {
                        JSValue v = popValue();     // need to have this sitting right above the base value
                        insertValue(v, mStackTop - dimCount);

                        JSValue result;
                        if (target->isNative())
                            result = target->getNativeCode()(this, *baseValue, baseValue, (dimCount + 2));
                        else {
                            uint32 argCount = dimCount + 2;
                            JSValue *argBase = buildArgumentBlock(target, argCount);
                            result = interpret(target->getByteCode(), 0, target->getScopeChain(), *baseValue, argBase, argCount);
                        }
                        resizeStack(stackSize() - (dimCount + 2));
                        pushValue(result);
                    }
                    else {  // XXX or should this be implemented in Object_Type as operator "[]=" ?
                        if (dimCount != 1)
                            reportError(Exception::typeError, "too many indices");
                        JSValue v = popValue();
                        JSValue index = popValue();
                        popValue();     // discard base
                        const String *name = index.toString(this).string;
                        obj->setProperty(this, *name, CURRENT_ATTR, v);
                        pushValue(v);                
                    }
                }
                break;
            case DeleteElementOp:
                {
                    uint32 dimCount = *((uint16 *)pc);
                    pc += sizeof(uint16);
                    mPC = pc;

                    JSValue *baseValue = getBase(stackSize() - (dimCount + 1));

                    // Use the type of the base to dispatch on...
                    JSObject *obj = NULL;
                    if (!baseValue->isObject() && !baseValue->isType())
                        obj = baseValue->toObject(this).object;
                    else
                        obj = baseValue->object;
                    JSFunction *target = obj->getType()->getUnaryOperator(DeleteIndex);

                    if (target) {
                        JSValue result;
                        if (target->isNative())
                            result = target->getNativeCode()(this, kNullValue, baseValue, dimCount + 1);
                        else {
                            uint32 argCount = dimCount + 1;
                            JSValue *argBase = buildArgumentBlock(target, argCount);
                            result = interpret(target->getByteCode(), 0, target->getScopeChain(), kNullValue, argBase, argCount);
                        }
                        resizeStack(stackSize() - (dimCount + 1));
                        pushValue(result);
                    }
                    else {  // XXX or should this be implemented in Object_Type as operator "delete[]" ?
                        if (dimCount != 1)
                            reportError(Exception::typeError, "too many indices");
                        JSValue index = popValue();
                        popValue();     // discard base
                        const String *name = index.toString(this).string;
                        PropertyIterator it;
                        if (obj->hasOwnProperty(*name, CURRENT_ATTR, Read, &it))
                            obj->deleteProperty(*name, CURRENT_ATTR);
                        pushValue(kTrueValue);
                    }
                }
                break;
            case GetPropertyOp:
                {
                    JSValue base = popValue();
                    JSObject *obj = NULL;
                    if (!base.isObject() && !base.isType())
                        obj = base.toObject(this).object;
                    else
                        obj = base.object;
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    mPC = pc;
                    const String &name = *mCurModule->getString(index);
                    obj->getProperty(this, name, CURRENT_ATTR);
                    // if the result is a function of some kind, bind
                    // the base object to it
                    JSValue result = topValue();
                    if (result.isFunction()) {
                        popValue();
                        if (result.function->isConstructor())
                            // A constructor has to be called with a NULL 'this' in order to prompt it
                            // to construct the instance object.
                            pushValue(JSValue((JSFunction *)(new JSBoundFunction(result.function, NULL))));
                        else
                            pushValue(JSValue((JSFunction *)(new JSBoundFunction(result.function, obj))));
                    }
                }
                break;
            case GetInvokePropertyOp:
                {
                    JSValue base = topValue();
                    JSObject *obj = NULL;
                    if (!base.isObject() && !base.isType() && !base.isFunction()) {
                        obj = base.toObject(this).object;
                        popValue();
                        pushValue(JSValue(obj)); // want the "toObject'd" version of base
                    }
                    else
                        obj = base.object;

                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    mPC = pc;
                    
                    const String &name = *mCurModule->getString(index);

//                    const String &name = *mCurModule->getIdentifierString(index);
//                    AttributeList *attr = mCurModule->getIdentifierAttr(index);
//                    attr->next = CURRENT_ATTR;

                    obj->getProperty(this, name, CURRENT_ATTR);
                }
                break;
            case SetPropertyOp:
                {
                    JSValue v = popValue();
                    JSValue base = popValue();
                    JSObject *obj = NULL;
                    if (!base.isObject() && !base.isType())
                        obj = base.toObject(this).object;
                    else
                        obj = base.object;
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    mPC = pc;
                    const String &name = *mCurModule->getString(index);
                    obj->setProperty(this, name, CURRENT_ATTR, v);
                    pushValue(v);
                }
                break;
            case DoUnaryOp:
                {
                    Operator op = (Operator)(*pc++);
                    mPC = pc;
                    JSValue v = topValue();
                    JSFunction *target;
                    if (v.isObject() && (target = v.object->getType()->getUnaryOperator(op)) )
                    {                    
                        uint32 argBase = stackSize() - 1;
                        JSValue newThis = kNullValue;
                        if (!target->isNative()) {
                            // lie about argCount to the activation since it
                            // would normally expect to clean the function pointer
                            // off the stack as well.
                            mActivationStack.push(new Activation(mLocals, mStack, mStackTop - 1, 
                                                                    mScopeChain,
                                                                    mArgumentBase, mThis,
                                                                    pc, mCurModule));
                            mThis = newThis;
                            mCurModule = target->getByteCode();
                            pc = mCurModule->mCodeBase;
                            endPC = mCurModule->mCodeBase + mCurModule->mLength;
                            mArgumentBase = getBase(argBase);
                            mLocals = new JSValue[mCurModule->mLocalsCount];
                            mStack = new JSValue[mCurModule->mStackDepth];
                            mStackMax = mCurModule->mStackDepth;
                            mStackTop = 0;
                        }
                        else {
                            JSValue result = (target->getNativeCode())(this, newThis, getBase(argBase), 0);
                            resizeStack(stackSize() -  1);
                            pushValue(result);
                        }
                        break;
                    }                    

                    switch (op) {
                    default:
                        NOT_REACHED("bad unary op");
                    case Increment: // defined in terms of '+'
                        {
                            pushValue(JSValue(1.0));
                            if (executeOperator(Plus, v.getType(), Number_Type)) {
                                // need to invoke
                                pc = mCurModule->mCodeBase;
                                endPC = mCurModule->mCodeBase + mCurModule->mLength;
                                mLocals = new JSValue[mCurModule->mLocalsCount];
                                mStack = new JSValue[mCurModule->mStackDepth];
                                mStackMax = mCurModule->mStackDepth;
                                mStackTop = 0;
                            }
                        }
                        break;
                    case Decrement: // defined in terms of '-'
                        {
                            pushValue(JSValue(1.0));
                            if (executeOperator(Minus, v.getType(), Number_Type)) {
                                // need to invoke
                                pc = mCurModule->mCodeBase;
                                endPC = mCurModule->mCodeBase + mCurModule->mLength;
                                mLocals = new JSValue[mCurModule->mLocalsCount];
                                mStack = new JSValue[mCurModule->mStackDepth];
                                mStackMax = mCurModule->mStackDepth;
                                mStackTop = 0;
                            }
                        }
                        break;
                    case Negate:
                        {
                            popValue();
                            JSValue n = v.toNumber(this);
                            if (n.isNaN())
                                pushValue(n);
                            else
                                pushValue(JSValue(-n.f64));
                        }
                        break;
                    case Posate:
                        {
                            popValue();
                            JSValue n = v.toNumber(this);
                            pushValue(n);
                        }
                        break;
                    case Complement:
                        {
                            popValue();
                            JSValue n = v.toInt32(this);
                            pushValue(JSValue((float64)(~(int32)(n.f64))));
                        }
                        break;
                    }
                }
                break;
            case DoOperatorOp:
                {
                    Operator op = (Operator)(*pc++);
                    mPC = pc;
                    JSValue v1 = getValue(stackSize() - 2);
                    JSValue v2 = getValue(stackSize() - 1);
                    if (executeOperator(op, v1.getType(), v2.getType())) {
                        // need to invoke
                        pc = mCurModule->mCodeBase;
                        endPC = mCurModule->mCodeBase + mCurModule->mLength;
                        mLocals = new JSValue[mCurModule->mLocalsCount];
                        mStack = new JSValue[mCurModule->mStackDepth];
                        mStackMax = mCurModule->mStackDepth;
                        mStackTop = 0;
                    }
                }
                break;
            case GetConstructorOp:
                {
                    JSValue v = popValue();
                    ASSERT(v.isType());
                    pushValue(JSValue(v.type->getDefaultConstructor()));
                }
                break;
            case NewInstanceOp:
                {
                    uint32 argCount = *((uint32 *)pc); 
                    pc += sizeof(uint32);
                    uint32 cleanUp = argCount;
                    JSValue *argBase = getBase(stackSize() - argCount);
                    bool isPrototypeFunctionCall = false;

                    JSFunction *target = NULL;
                    JSValue newThis = kNullValue;
                    JSValue *typeValue = getBase(stackSize() - (argCount + 1));
                    if (!typeValue->isType()) {                        
                        if (typeValue->isFunction() && typeValue->function->isPrototype()) {
                            isPrototypeFunctionCall = true;
                            target = typeValue->function;
                            newThis = Object_Type->newInstance(this);
                            PropertyIterator i;
                            if (target->hasProperty(widenCString("prototype"), CURRENT_ATTR, Read, &i)) {
                                JSValue v = target->getPropertyValue(i);
                                newThis.object->mPrototype = v.toObject(this).object;
                            }
                        }
                        else            
                            reportError(Exception::referenceError, "Not a type or a prototype function");
                    }
                    else {
                        // if the type has an operator "new" use that, 
                        // otherwise use the default constructor (and pass NULL
                        // for the this value)
                        target = typeValue->type->getUnaryOperator(New);
                        if (target)
                            newThis = JSValue(typeValue->type->newInstance(this));
                        else {
                            newThis = kNullValue;
                            target = typeValue->type->getDefaultConstructor();
                        }
                    }                    
                    ASSERT(target);

                    JSValue result;
                    if (target->isNative())
                        result = target->getNativeCode()(this, newThis, argBase, argCount);
                    else {
                        argBase = buildArgumentBlock(target, argCount);
                        result = interpret(target->getByteCode(), 0, target->getScopeChain(), newThis, argBase, argCount);
                    }
                    resizeStack(stackSize() - cleanUp);
                    
                    if (!isPrototypeFunctionCall)
                        // If it's a prototype function, we don't care what it returns, 
                        // we have the 'this' already
                        // otherwise, constructor has potentially made the 'this', so retain it
                        newThis = result;

                    popValue();             // don't need the type anymore
                    pushValue(newThis);
                 }
                break;
            case NewThisOp:
                {
                    JSValue v = popValue();
                    if (mThis.isNull()) {
                        ASSERT(v.isType());
                        mThis = JSValue(v.type->newInstance(this));
                    }
                }
                break;
            case NewObjectOp:
                {
                    pushValue(JSValue(Object_Type->newInstance(this)));
                }
                break;
            case GetLocalVarOp:
                {
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    pushValue(mLocals[index]);
                }
                break;
            case SetLocalVarOp:
                {
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    mLocals[index] = topValue();
                }
                break;
            case GetClosureVarOp:
                {
                    uint32 depth = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
//                    pushValue(mScopeChain->getClosureVar(depth, index));                    
                }
                break;
            case SetClosureVarOp:
                {
                    uint32 depth = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
//                    mScopeChain->setClosureVar(depth, index, topValue()));
                }
                break;
            case NewClosureOp:
                {
                }
                break;
            case LoadThisOp:
                {
                    pushValue(mThis);
                }
                break;
            case GetArgOp:
                {
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    pushValue(mArgumentBase[index]);
                }
                break;
            case SetArgOp:
                {
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    mArgumentBase[index] = topValue();
                }
                break;
            case GetMethodOp:
                {
                    JSValue base = topValue();
                    ASSERT(dynamic_cast<JSInstance *>(base.object));
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    pushValue(JSValue(base.object->mType->mMethods[index]));
                }
                break;
            case GetMethodRefOp:
                {
                    JSValue base = popValue();
                    ASSERT(dynamic_cast<JSInstance *>(base.object));
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    pushValue(JSValue(new JSBoundFunction(base.object->mType->mMethods[index], base.object)));
                }
                break;
            case GetFieldOp:
                {
                    JSValue base = popValue();
                    ASSERT(dynamic_cast<JSInstance *>(base.object));
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    pushValue(((JSInstance *)(base.object))->mInstanceValues[index]);
                }
                break;
            case SetFieldOp:
                {
                    JSValue v = popValue();
                    JSValue base = popValue();
                    ASSERT(dynamic_cast<JSInstance *>(base.object));
                    uint32 index = *((uint32 *)pc);
                    pc += sizeof(uint32);
                    ((JSInstance *)(base.object))->mInstanceValues[index] = v;
                    pushValue(v);
                }
                break;
            case WithinOp:
                {
                    JSValue base = popValue();
                    mScopeChain->addScope(base.toObject(this).object);
                }
                break;
            case WithoutOp:
                {
                    mScopeChain->popScope();
                }
                break;
            case PushScopeOp:
                {
                    JSObject *obj = *((JSObject **)pc);
                    mScopeChain->addScope(obj);
                    pc += sizeof(JSObject *);
                }
                break;
            case PopScopeOp:
                {
                    mScopeChain->popScope();
                }
                break;
            case LoadGlobalObjectOp:
                {
                    pushValue(JSValue(getGlobalObject()));
                }
                break;
            case JsrOp:
                {
                    uint32 offset = *((uint32 *)pc);
                    mSubStack.push(pc + sizeof(uint32));
                    pc += offset;
                }
                break;
            case RtsOp:
                {
                    pc = mSubStack.top();
                    mSubStack.pop();
                }
                break;

            case TryOp:
                {
                    Activation *curAct = (mActivationStack.size() > 0) ? mActivationStack.top() : NULL;
                    uint32 handler = *((uint32 *)pc);
                    if (handler != toUInt32(-1))
                        mTryStack.push(new HandlerData(pc + handler, stackSize(), curAct));
                    pc += sizeof(uint32);
                    handler = *((uint32 *)pc);
                    if (handler != toUInt32(-1))
                        mTryStack.push(new HandlerData(pc + handler, stackSize(), curAct));
                    pc += sizeof(uint32);
                }
                break;
            case HandlerOp:
                {
                    HandlerData *hndlr = (HandlerData *)mTryStack.top();
                    mTryStack.pop();
                    delete hndlr;
                }
                break;
            case ThrowOp:
                {   
                    JSValue x = topValue();
                    if (mTryStack.size() > 0) {
                        HandlerData *hndlr = (HandlerData *)mTryStack.top();
                        Activation *curAct = (mActivationStack.size() > 0) ? mActivationStack.top() : NULL;
                        if (curAct != hndlr->mActivation) {
                            Activation *prev = mActivationStack.top();
                            do {
                                mActivationStack.pop();
                                curAct = mActivationStack.top();                            
                            } while (hndlr->mActivation != curAct);
                            mCurModule = prev->mModule;
                            endPC = mCurModule->mCodeBase + mCurModule->mLength;
                            mLocals = prev->mLocals;
                            mStack = prev->mStack;
                            mStackTop = 1;          // just the exception object remains
                            mStackMax = mCurModule->mStackDepth;
                            mArgumentBase = prev->mArgumentBase;
                            mThis = prev->mThis;
                        }

                        resizeStack(hndlr->mStackSize);
                        pc = hndlr->mPC;
                        pushValue(x);
                    }
                    else
                        reportError(Exception::uncaughtError, "No handler for throw");
                }
                break;
            case ClassOp:
                {
                    JSValue x = popValue();
                    ASSERT(x.isObject());
                    pushValue(JSValue(x.getType()));
                }
                break;
            case JuxtaposeOp:
                {
                    JSValue v2 = popValue();
                    JSValue v1 = popValue();

                    ASSERT(v1.isObject() && (v1.object->getType() == Attribute_Type));
                    ASSERT(v2.isObject() && (v2.object->getType() == Attribute_Type));

                    Attribute *a1 = static_cast<Attribute *>(v1.object);
                    Attribute *a2 = static_cast<Attribute *>(v2.object);

                    if ((a1->mTrueFlags & a2->mFalseFlags) != 0)
                        reportError(Exception::semanticError, "Mismatched attributes"); // XXX could supply more detail
                    if ((a1->mFalseFlags & a2->mTrueFlags) != 0)
                        reportError(Exception::semanticError, "Mismatched attributes");

                    
                    // Now build the result attribute and set it's values
                    Attribute *x = new Attribute(a1->mTrueFlags | a2->mTrueFlags, a1->mFalseFlags | a2->mFalseFlags);

                    NamespaceList *t = a1->mNamespaceList;
                    while (t) {
                        x->mNamespaceList = new NamespaceList(&t->mName, x->mNamespaceList);
                        t = t->mNext;
                    }
                    t = a2->mNamespaceList;
                    while (t) {
                        x->mNamespaceList = new NamespaceList(&t->mName, x->mNamespaceList);
                        t = t->mNext;
                    }
                    if (a1->mExtendArgument)
                        x->mExtendArgument = a1->mExtendArgument;
                    else
                        if (a2->mExtendArgument)
                            x->mExtendArgument = a2->mExtendArgument;
                    
                    pushValue(JSValue(x));
                }
                break;
            case CastOp:
                {
                    JSValue t = popValue();
                    ASSERT(t.isType());
                    JSType *toType = t.type;
                    JSValue v = popValue();

                    pushValue(mapValueToType(v, toType));
                }
                break;

            default:
                reportError(Exception::internalError, "Bad Opcode");
            }
        }
        catch (Exception x) {
            throw x;
        }
    }
    return result;
}


static JSValue numberPlus(Context *, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue(r1.f64 + r2.f64);
}

static JSValue integerPlus(Context * /*cx*/, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue(r1.f64 + r2.f64);
}

static JSValue objectPlus(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    if (r1.isNumber() && r2.isNumber()) {        
        // this is the ECMA3 implementation. Suppose somebody has
        // added (can they?) an operator "+" for Numbers - we should
        // oughta dispatch to that function here. (And all other
        // cases below, arrgh - how deep does this go? my brain hurts)
        return JSValue(r1.toNumber(cx).f64 + r2.toNumber(cx).f64);
    }

    if (r1.isString()) {
        if (r2.isString())
            return JSValue(new String(*r1.string + *r2.string));
        else
            return JSValue(new String(*r1.string + *r2.toString(cx).string));
    }
    else {
        if (r2.isString()) 
            return JSValue(new String(*r1.toString(cx).string + *r2.string));
        else {
            JSValue r1p = r1.toPrimitive(cx);
            JSValue r2p = r2.toPrimitive(cx);
            if (r1p.isString() || r2p.isString()) {
                if (r1p.isString())
                    if (r2p.isString())
                        return JSValue(new String(*r1p.string + *r2p.string));
                    else
                        return JSValue(new String(*r1p.string + *r2p.toString(cx).string));
                else
                    return JSValue(new String(*r1p.toString(cx).string + *r2p.string));
            }
            else {
                JSValue num1(r1.toNumber(cx));
                JSValue num2(r2.toNumber(cx));
                return JSValue(num1.f64 + num2.f64);
            }
        }
    }
}



static JSValue integerMinus(Context * /*cx*/, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue(r1.f64 - r2.f64);
}

static JSValue numberMinus(Context *, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue(r1.f64 - r2.f64);
}

static JSValue objectMinus(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue(r1.toNumber(cx).f64 - r2.toNumber(cx).f64);
}



static JSValue integerMultiply(Context * /*cx*/, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue(r1.f64 * r2.f64);
}

static JSValue objectMultiply(Context * /*cx*/, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue(r1.f64 * r2.f64);
}



static JSValue integerDivide(Context * /*cx*/, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    float64 d = r1.f64 / r2.f64;
    bool neg = (d < 0);
    d = fd::floor(neg ? -d : d);
    d = neg ? -d : d;
    return JSValue(d);
}

static JSValue objectDivide(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue(r1.toNumber(cx).f64 / r2.toNumber(cx).f64);
}



static JSValue integerRemainder(Context * /*cx*/, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    float64 d = fd::fmod(r1.f64, r2.f64);
    bool neg = (d < 0);
    d = fd::floor(neg ? -d : d);
    d = neg ? -d : d;
    return JSValue(d);
}

static JSValue objectRemainder(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue(fd::fmod(r1.toNumber(cx).f64, r2.toNumber(cx).f64));
}



static JSValue integerShiftLeft(Context * /*cx*/, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue((float64)( (int32)(r1.f64) << ( (uint32)(r2.f64) & 0x1F)) );
}

static JSValue objectShiftLeft(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue((float64)( (int32)(r1.toInt32(cx).f64) << ( (uint32)(r2.toUInt32(cx).f64) & 0x1F)) );
}



static JSValue integerShiftRight(Context * /*cx*/, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue((float64) ( (int32)(r1.f64) >> ( (uint32)(r2.f64) & 0x1F)) );
}

static JSValue objectShiftRight(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue((float64) ( (int32)(r1.toInt32(cx).f64) >> ( (uint32)(r2.toUInt32(cx).f64) & 0x1F)) );
}



static JSValue integerUShiftRight(Context * /*cx*/, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue((float64) ( (uint32)(r1.f64) >> ( (uint32)(r2.f64) & 0x1F)) );
}

static JSValue objectUShiftRight(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue((float64) ( (uint32)(r1.toUInt32(cx).f64) >> ( (uint32)(r2.toUInt32(cx).f64) & 0x1F)) );
}



static JSValue integerBitAnd(Context * /*cx*/, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue((float64)( (int32)(r1.f64) & (int32)(r2.f64) ));
}

static JSValue objectBitAnd(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue((float64)( (int32)(r1.toInt32(cx).f64) & (int32)(r2.toInt32(cx).f64) ));
}



static JSValue integerBitXor(Context * /*cx*/, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue((float64)( (int32)(r1.f64) ^ (int32)(r2.f64) ));
}

static JSValue objectBitXor(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue((float64)( (int32)(r1.toInt32(cx).f64) ^ (int32)(r2.toInt32(cx).f64) ));
}



static JSValue integerBitOr(Context * /*cx*/, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue((float64)( (int32)(r1.f64) | (int32)(r2.f64) ));
}

static JSValue objectBitOr(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    return JSValue((float64)( (int32)(r1.toInt32(cx).f64) | (int32)(r2.toInt32(cx).f64) ));
}



//
// implements r1 < r2, returning true or false or undefined
//
static JSValue objectCompare(Context *cx, JSValue &r1, JSValue &r2)
{
    JSValue r1p = r1.toPrimitive(cx, JSValue::NumberHint);
    JSValue r2p = r2.toPrimitive(cx, JSValue::NumberHint);

    if (r1p.isString() && r2p.isString())
        return JSValue(bool(r1p.string->compare(*r2p.string) < 0));
    else {
        JSValue r1n = r1p.toNumber(cx);
        JSValue r2n = r2p.toNumber(cx);
        if (r1n.isNaN() || r2n.isNaN())
            return kUndefinedValue;
        else
            return JSValue(r1n.f64 < r2n.f64);
    }

}

static JSValue objectLess(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    JSValue result = objectCompare(cx, r1, r2);
    if (result.isUndefined())
        return kFalseValue;
    else
        return result;
}

static JSValue objectLessEqual(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue &r1 = argv[0];
    JSValue &r2 = argv[1];
    JSValue result = objectCompare(cx, r2, r1);
    if (result.isTrue() || result.isUndefined())
        return kFalseValue;
    else
        return kTrueValue;
}

static JSValue compareEqual(Context *cx, JSValue r1, JSValue r2)
{
    if (r1.getType() != r2.getType()) {
        if (r1.isNull() && r2.isUndefined())
            return kTrueValue;
        if (r1.isUndefined() && r2.isNull())
            return kTrueValue;
        if (r1.isNumber() && r2.isString())
            return compareEqual(cx, r1, r2.toNumber(cx));
        if (r1.isString() && r2.isNumber())
            return compareEqual(cx, r1.toNumber(cx), r2.toString(cx));
        if (r1.isBool())
            return compareEqual(cx, r1.toNumber(cx), r2);
        if (r2.isBool())
            return compareEqual(cx, r1, r2.toNumber(cx));
        if ( (r1.isString() || r1.isNumber()) && (r2.isObject()) )
            return compareEqual(cx, r1, r2.toPrimitive(cx));
        if ( (r1.isObject()) && (r2.isString() || r2.isNumber()) )
            return compareEqual(cx, r1.toPrimitive(cx), r2);
        return kFalseValue;
    }
    else {
        if (r1.isUndefined())
            return kTrueValue;
        if (r1.isNull())
            return kTrueValue;
        if (r1.isNumber()) {
            if (r1.isNaN())
                return kFalseValue;
            if (r2.isNaN())
                return kFalseValue;
            return JSValue(r1.f64 == r2.f64);
        }
        else {
            if (r1.isString())
                return JSValue(bool(r1.string->compare(*r2.string) == 0));
            if (r1.isBool())
                return JSValue(r1.boolean == r2.boolean);
            if (r1.isObject())
                return JSValue(r1.object == r2.object);
            if (r1.isType())
                return JSValue(r1.type == r2.type);
            if (r1.isFunction())
                return JSValue(r1.function->isEqual(r2.function));
            NOT_REACHED("unhandled type");
            return kFalseValue;
        }
    }
}

static JSValue objectEqual(Context *cx, const JSValue& /*thisValue*/, JSValue *argv, uint32 /*argc*/)
{
    JSValue r1 = argv[0];
    JSValue r2 = argv[1];
    
    return compareEqual(cx, r1, r2);
}


void Context::initOperators()
{
    struct OpTableEntry {
        Operator which;
        JSType *op1;
        JSType *op2;
        JSFunction::NativeCode *imp;
        JSType *resType;
    } OpTable[] = {
        { Plus,  Object_Type, Object_Type, objectPlus,  Object_Type },
        { Plus,  Number_Type, Number_Type, numberPlus,  Number_Type },
        { Plus,  Integer_Type, Integer_Type, integerPlus,  Integer_Type },

        { Minus, Object_Type, Object_Type, objectMinus, Number_Type },
        { Minus, Number_Type, Number_Type, numberMinus, Number_Type },
        { Minus,  Integer_Type, Integer_Type, integerMinus,  Integer_Type },

        { ShiftLeft, Integer_Type, Integer_Type, integerShiftLeft, Integer_Type },
        { ShiftLeft, Object_Type, Object_Type, objectShiftLeft, Number_Type },

        { ShiftRight, Integer_Type, Integer_Type, integerShiftRight, Integer_Type },
        { ShiftRight, Object_Type, Object_Type, objectShiftRight, Number_Type },

        { UShiftRight, Integer_Type, Integer_Type, integerUShiftRight, Integer_Type },
        { UShiftRight, Object_Type, Object_Type, objectUShiftRight, Number_Type },
        
        { BitAnd, Integer_Type, Integer_Type, integerBitAnd, Integer_Type },
        { BitAnd, Object_Type, Object_Type, objectBitAnd, Number_Type },
        
        { BitXor, Integer_Type, Integer_Type, integerBitXor, Integer_Type },
        { BitXor, Object_Type, Object_Type, objectBitXor, Number_Type },

        { BitOr, Integer_Type, Integer_Type, integerBitOr, Integer_Type },
        { BitOr, Object_Type, Object_Type, objectBitOr, Number_Type },

        { Multiply, Integer_Type, Integer_Type, integerMultiply, Integer_Type },
        { Multiply, Object_Type, Object_Type, objectMultiply, Number_Type },
        
        { Divide, Integer_Type, Integer_Type, integerDivide, Integer_Type },
        { Divide, Object_Type, Object_Type, objectDivide, Number_Type },

        { Remainder, Integer_Type, Integer_Type, integerRemainder, Integer_Type },
        { Remainder, Object_Type, Object_Type, objectRemainder, Number_Type },

        { Less, Object_Type, Object_Type, objectLess, Boolean_Type },
        { LessEqual, Object_Type, Object_Type, objectLessEqual, Boolean_Type },

        { Equal, Object_Type, Object_Type, objectEqual, Boolean_Type },
    };

    for (uint32 i = 0; i < sizeof(OpTable) / sizeof(OpTableEntry); i++) {
        JSFunction *f = new JSFunction(OpTable[i].imp, OpTable[i].resType);
        OperatorDefinition *op = new OperatorDefinition(OpTable[i].op1, OpTable[i].op2, f);
        mOperatorTable[OpTable[i].which].push_back(op);
    }
}


JSValue JSValue::valueToObject(Context *cx, const JSValue& value)
{
    switch (value.tag) {
    case f64_tag:
        {
            JSObject *obj = Number_Type->newInstance(cx);
            JSFunction *defCon = Number_Type->getDefaultConstructor();
            JSValue argv[1];
            JSValue thisValue = JSValue(obj);
            argv[0] = value;
            if (defCon->isNative()) {
                (defCon->getNativeCode())(cx, thisValue, &argv[0], 1); 
            }
            else {
                ASSERT(false);  // need to throw a hot potato back to
                                // ye interpreter loop
            }
            return thisValue;
        }
    case boolean_tag:
        {
            JSObject *obj = Boolean_Type->newInstance(cx);
            JSFunction *defCon = Boolean_Type->getDefaultConstructor();
            JSValue argv[1];
            JSValue thisValue = JSValue(obj);
            argv[0] = value;
            if (defCon->isNative()) {
                (defCon->getNativeCode())(cx, thisValue, &argv[0], 1); 
            }
            else {
                ASSERT(false);
            }
            return thisValue;
        }
    case string_tag: 
        {
            JSObject *obj = String_Type->newInstance(cx);
            JSFunction *defCon = String_Type->getDefaultConstructor();
            JSValue argv[1];
            JSValue thisValue = JSValue(obj);
            argv[0] = value;
            if (defCon->isNative()) {
                (defCon->getNativeCode())(cx, thisValue, &argv[0], 1); 
            }
            else {
                ASSERT(false);
            }
            return thisValue;
        }
    case object_tag:
    case function_tag:
        return value;
    case null_tag:
    case undefined_tag:
        cx->reportError(Exception::typeError, "ToObject");
    default:
        NOT_REACHED("Bad tag");
        return kUndefinedValue;
    }
}

float64 stringToNumber(const String *string)
{
    const char16 *numEnd;
    return stringToDouble(string->begin(), string->end(), numEnd);
}

JSValue JSValue::valueToNumber(Context *cx, const JSValue& value)
{
    switch (value.tag) {
    case f64_tag:
        return value;
    case string_tag: 
        return JSValue(stringToNumber(value.string));
    case object_tag:
    case function_tag:
        return value.toPrimitive(cx, NumberHint).toNumber(cx);
    case boolean_tag:
        return JSValue((value.boolean) ? 1.0 : 0.0);
    case undefined_tag:
        return kNaNValue;
    default:
        NOT_REACHED("Bad tag");
        return kUndefinedValue;
    }
}

String *numberToString(float64 number)
{
    char buf[dtosStandardBufferSize];
    const char *chrp = doubleToStr(buf, dtosStandardBufferSize, number, dtosStandard, 0);
    return new JavaScript::String(widenCString(chrp));
}
              
JSValue JSValue::valueToString(Context *cx, const JSValue& value)
{
    const String *strp = NULL;
    JSObject *obj = NULL;
    switch (value.tag) {
    case f64_tag:
        return JSValue(numberToString(value.f64));
    case object_tag:
        obj = value.object;
        break;
    case function_tag:
        obj = value.function;
        break;
    case string_tag:
        return value;
    case boolean_tag:
        strp = (value.boolean) 
                        ? new JavaScript::String(widenCString("true")) 
                        : new JavaScript::String(widenCString("false"));
        break;
    case type_tag:
        strp = value.type->mClassName;
        break;
    case undefined_tag:
        strp = new JavaScript::String(widenCString("undefined"));
        break;
    case null_tag:
        strp = new JavaScript::String(widenCString("null"));
        break;
    default:
        NOT_REACHED("Bad tag");
    }
    if (obj) {
        JSFunction *target = NULL;
        PropertyIterator i;
        if (obj->hasProperty(widenCString("toString"), CURRENT_ATTR, Read, &i)) {
            JSValue v = obj->getPropertyValue(i);
            if (v.isFunction())
                target = v.function;
        }
        if (target == NULL) {
            if (obj->hasProperty(widenCString("valueOf"), CURRENT_ATTR, Read, &i)) {
                JSValue v = obj->getPropertyValue(i);
                if (v.isFunction())
                    target = v.function;
            }
        }
        if (target) {
            if (!target->isNative()) {
                // here we need to get the interpreter to do the job
                ASSERT(false);
            }
            else
                return (target->getNativeCode())(cx, value, NULL, 0);
        }
        throw new Exception(Exception::runtimeError, "toString");    // XXX
        return kUndefinedValue; // keep compilers happy
    }
    else
        return JSValue(strp);

}

JSValue JSValue::toPrimitive(Context *, Hint) const
{
    JSObject *obj;
    switch (tag) {
    case f64_tag:
    case string_tag:
    case boolean_tag:
    case undefined_tag:
        return *this;

    case object_tag:
        obj = object;
        break;
    case function_tag:
        obj = function;
        break;

    default:
        NOT_REACHED("Bad tag");
        return kUndefinedValue;
    }
/*
    JSFunction *target = NULL;
    JSValue result;
    JSValues argv(1);
    argv[0] = *this;

    // The following is [[DefaultValue]]
    //
    if ((hint == NumberHint) || (hint == NoHint)) {
        const JSValue &valueOf = obj->getProperty(widenCString("valueOf"));
        if (valueOf.isFunction()) {
            target = valueOf.function;
            if (target->isNative()) {
                result = static_cast<JSNativeFunction*>(target)->mCode(cx, argv);
            }
            else {
                Context new_cx(cx);
                result = new_cx.interpret(target->getICode(), argv);
            }
            if (result.isPrimitive())
                return result;
        }
        const JSValue &toString = obj->getProperty(widenCString("toString"));
        if (toString.isFunction()) {
            target = toString.function;
            if (target->isNative()) {
                result = static_cast<JSNativeFunction*>(target)->mCode(cx, argv);
            }
            else {
                Context new_cx(cx);
                result = new_cx.interpret(target->getICode(), argv);
            }
            if (result.isPrimitive())
                return result;
        }
    }
    else {
        const JSValue &toString = obj->getProperty(widenCString("toString"));
        if (toString.isFunction()) {
            target = toString.function;
            if (target->isNative()) {
                result = static_cast<JSNativeFunction*>(target)->mCode(cx, argv);
            }
            else {
                Context new_cx(cx);
                result = new_cx.interpret(target->getICode(), argv);
            }
            if (result.isPrimitive())
                return result;
        }
        const JSValue &valueOf = obj->getProperty(widenCString("valueOf"));
        if (valueOf.isFunction()) {
            target = valueOf.function;
            if (target->isNative()) {
                result = static_cast<JSNativeFunction*>(target)->mCode(cx, argv);
            }
            else {
                Context new_cx(cx);
                result = new_cx.interpret(target->getICode(), argv);
            }
            if (result.isPrimitive())
                return result;
        }
    }
    throw Exception(Exception::runtimeError, "toPrimitive");    // XXX
*/
    return kUndefinedValue;
    
}

int JSValue::operator==(const JSValue& value) const
{
    if (this->tag == value.tag) {
#       define CASE(T) case T##_tag: return (this->T == value.T)
        switch (tag) {
        CASE(f64);
        CASE(object);
        CASE(boolean);
        #undef CASE
        // question:  are all undefined values equal to one another?
        case undefined_tag: return 1;
        default:
            NOT_REACHED("Broken compiler?");            
        }
    }
    return 0;
}


JSValue JSValue::valueToInt32(Context *, const JSValue& value)
{
    float64 d;
    switch (value.tag) {
    case f64_tag:
        d = value.f64;
        break;
    case string_tag: 
        {
            const char16 *numEnd;
            d = stringToDouble(value.string->begin(), value.string->end(), numEnd);
        }
        break;
    case boolean_tag:
        return JSValue((float64)((value.boolean) ? 1 : 0));
    case object_tag:
    case undefined_tag:
        // toNumber(toPrimitive(hint Number))
        return kUndefinedValue;
    default:
        NOT_REACHED("Bad tag");
        return kUndefinedValue;
    }
    if ((d == 0.0) || !JSDOUBLE_IS_FINITE(d) )
        return JSValue((float64)0);
    d = fd::fmod(d, two32);
    d = (d >= 0) ? d : d + two32;
    if (d >= two31)
        return JSValue((float64)(d - two32));
    else
        return JSValue((float64)d);    
}

JSValue JSValue::valueToUInt32(Context *, const JSValue& value)
{
    float64 d;
    switch (value.tag) {
    case f64_tag:
        d = value.f64;
        break;
    case string_tag: 
        {
            const char16 *numEnd;
            d = stringToDouble(value.string->begin(), value.string->end(), numEnd);
        }
        break;
    case boolean_tag:
        return JSValue((float64)((value.boolean) ? 1 : 0));
    case object_tag:
    case undefined_tag:
        // toNumber(toPrimitive(hint Number))
        return kUndefinedValue;
    default:
        NOT_REACHED("Bad tag");
        return kUndefinedValue;
    }
    if ((d == 0.0) || !JSDOUBLE_IS_FINITE(d))
        return JSValue((float64)0);
    bool neg = (d < 0);
    d = fd::floor(neg ? -d : d);
    d = neg ? -d : d;
    d = fd::fmod(d, two32);
    d = (d >= 0) ? d : d + two32;
    return JSValue((float64)d);
}

JSValue JSValue::valueToUInt16(Context *, const JSValue& value)
{
    float64 d;
    switch (value.tag) {
    case f64_tag:
        d = value.f64;
        break;
    case string_tag: 
        {
            const char16 *numEnd;
            d = stringToDouble(value.string->begin(), value.string->end(), numEnd);
        }
        break;
    case boolean_tag:
        return JSValue((float64)((value.boolean) ? 1 : 0));
    case object_tag:
    case undefined_tag:
        // toNumber(toPrimitive(hint Number))
        return kUndefinedValue;
    default:
        NOT_REACHED("Bad tag");
        return kUndefinedValue;
    }
    if ((d == 0.0) || !JSDOUBLE_IS_FINITE(d))
        return JSValue((float64)0);
    bool neg = (d < 0);
    d = fd::floor(neg ? -d : d);
    d = neg ? -d : d;
    d = fd::fmod(d, two16);
    d = (d >= 0) ? d : d + two16;
    return JSValue((float64)d);
}

JSValue JSValue::valueToBoolean(Context *cx, const JSValue& value)
{
    JSObject *obj = NULL;
    switch (value.tag) {
    case f64_tag:
        return JSValue(!(value.f64 == 0.0) || JSDOUBLE_IS_NaN(value.f64));
    case string_tag: 
        return JSValue(value.string->length() != 0);
    case boolean_tag:
        return value;
    case object_tag:
        obj = value.object;
        break;
    case function_tag:
        obj = value.function;
        break;
    case undefined_tag:
        return kFalseValue;
    default:
        NOT_REACHED("Bad tag");
        return kUndefinedValue;
    }
    ASSERT(obj);
    JSFunction *target = NULL;
    PropertyIterator i;
    if (obj->hasProperty(widenCString("toBoolean"), CURRENT_ATTR, Read, &i)) {
        JSValue v = obj->getPropertyValue(i);
        if (v.isFunction())
            target = v.function;
    }
    if (target) {
        if (!target->isNative()) {
            // here we need to get the interpreter to do the job
            ASSERT(false);
        }
        else {
            JSValue args = value;
            return (target->getNativeCode())(cx, value, &args, 1);
        }
    }
    throw new Exception(Exception::runtimeError, "toBoolean");    // XXX
}


// See if 'v' can be represented as a 't' - works for
// the built-ins by falling back to the ECMA3 behaviour
JSValue Context::mapValueToType(JSValue v, JSType *t)
{
    // user conversions?
    if (v.getType() == t)
        return v;
    
    if (t == Number_Type) {
        return v.toNumber(this);
    }
    else
    if (t == Integer_Type) {
        float64 d = v.toNumber(this).f64;
        bool neg = (d < 0);
        d = fd::floor(neg ? -d : d);
        d = neg ? -d : d;
        return JSValue(d);
    }
    else
    if (t == String_Type) {
        return v.toString(this);
    }
    else
    if (t == Boolean_Type) {
        return v.toBoolean(this);
    }

    if (v.isUndefined())
        return v;

    if (v.getType()->derivesFrom(t))
        return v;

    reportError(Exception::typeError, "Invalid type cast");
    return kUndefinedValue;
}


    
}
}

