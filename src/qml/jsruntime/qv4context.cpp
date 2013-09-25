/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QString>
#include "qv4debugging_p.h"
#include <qv4context_p.h>
#include <qv4object_p.h>
#include <qv4objectproto_p.h>
#include "qv4mm_p.h"
#include <qv4argumentsobject_p.h>
#include "qv4function_p.h"
#include "qv4errorobject_p.h"

using namespace QV4;

CallContext *ExecutionContext::newCallContext(void *stackSpace, Value *locals, FunctionObject *function, CallData *callData)
{
    CallContext *c = (CallContext *)stackSpace;
#ifndef QT_NO_DEBUG
    c->next = (CallContext *)0x1;
#endif

    engine->current = c;

    c->initBaseContext(Type_CallContext, engine, this);

    c->function = function;
    c->callData = callData;
    c->realArgumentCount = callData->argc;

    c->strictMode = function->strictMode;
    c->marked = false;
    c->outer = function->scope;
#ifndef QT_NO_DEBUG
    assert(c->outer->next != (ExecutionContext *)0x1);
#endif

    c->activation = 0;

    if (function->function) {
        c->compilationUnit = function->function->compilationUnit;
        c->lookups = c->compilationUnit->runtimeLookups;
    }

    c->locals = locals;

    if (function->varCount)
        std::fill(c->locals, c->locals + function->varCount, Primitive::undefinedValue());

    if (callData->argc < function->formalParameterCount) {
#ifndef QT_NO_DEBUG
        Q_ASSERT(function->formalParameterCount <= QV4::Global::ReservedArgumentCount);
#endif
        std::fill(c->callData->args + callData->argc, c->callData->args + function->formalParameterCount, Primitive::undefinedValue());
        c->callData->argc = function->formalParameterCount;
    }

    return c;
}

CallContext *ExecutionContext::newCallContext(FunctionObject *function, CallData *callData)
{
    CallContext *c = static_cast<CallContext *>(engine->memoryManager->allocContext(requiredMemoryForExecutionContect(function, callData->argc)));

    engine->current = c;

    c->initBaseContext(Type_CallContext, engine, this);

    c->function = function;
    c->realArgumentCount = callData->argc;

    c->strictMode = function->strictMode;
    c->marked = false;
    c->outer = function->scope;
#ifndef QT_NO_DEBUG
    assert(c->outer->next != (ExecutionContext *)0x1);
#endif

    c->activation = 0;

    if (function->function) {
        c->compilationUnit = function->function->compilationUnit;
        c->lookups = c->compilationUnit->runtimeLookups;
    }

    c->locals = (Value *)(c + 1);

    if (function->varCount)
        std::fill(c->locals, c->locals + function->varCount, Primitive::undefinedValue());

    c->callData = reinterpret_cast<CallData *>(c->locals + function->varCount);
    ::memcpy(c->callData, callData, sizeof(CallData) + (callData->argc - 1) * sizeof(Value));
    if (callData->argc < function->formalParameterCount)
        std::fill(c->callData->args + c->callData->argc, c->callData->args + function->formalParameterCount, Primitive::undefinedValue());
    c->callData->argc = qMax((uint)callData->argc, function->formalParameterCount);

    return c;
}

WithContext *ExecutionContext::newWithContext(Object *with)
{
    WithContext *w = static_cast<WithContext *>(engine->memoryManager->allocContext(sizeof(WithContext)));
    engine->current = w;
    w->initWithContext(this, with);
    return w;
}

CatchContext *ExecutionContext::newCatchContext(String *exceptionVarName, const Value &exceptionValue)
{
    CatchContext *c = static_cast<CatchContext *>(engine->memoryManager->allocContext(sizeof(CatchContext)));
    engine->current = c;
    c->initCatchContext(this, exceptionVarName, exceptionValue);
    return c;
}

CallContext *ExecutionContext::newQmlContext(FunctionObject *f, Object *qml)
{
    CallContext *c = static_cast<CallContext *>(engine->memoryManager->allocContext(requiredMemoryForExecutionContect(f, 0)));

    engine->current = c;
    c->initQmlContext(this, qml, f);

    return c;
}



void ExecutionContext::createMutableBinding(const StringRef name, bool deletable)
{
    Scope scope(this);

    // find the right context to create the binding on
    Object *activation = engine->globalObject;
    ExecutionContext *ctx = this;
    while (ctx) {
        if (ctx->type >= Type_CallContext) {
            CallContext *c = static_cast<CallContext *>(ctx);
            if (!c->activation)
                c->activation = engine->newObject()->getPointer();
            activation = c->activation;
            break;
        }
        ctx = ctx->outer;
    }

    if (activation->__hasProperty__(name))
        return;
    Property desc = Property::fromValue(Primitive::undefinedValue());
    PropertyAttributes attrs(Attr_Data);
    attrs.setConfigurable(deletable);
    activation->__defineOwnProperty__(this, name, desc, attrs);
}

String * const *ExecutionContext::formals() const
{
    return type >= Type_CallContext ? static_cast<const CallContext *>(this)->function->formalParameterList : 0;
}

unsigned int ExecutionContext::formalCount() const
{
    return type >= Type_CallContext ? static_cast<const CallContext *>(this)->function->formalParameterCount : 0;
}

String * const *ExecutionContext::variables() const
{
    return type >= Type_CallContext ? static_cast<const CallContext *>(this)->function->varList : 0;
}

unsigned int ExecutionContext::variableCount() const
{
    return type >= Type_CallContext ? static_cast<const CallContext *>(this)->function->varCount : 0;
}


void GlobalContext::initGlobalContext(ExecutionEngine *eng)
{
    initBaseContext(Type_GlobalContext, eng, /*parentContext*/0);
    callData = reinterpret_cast<CallData *>(this + 1);
    callData->tag = QV4::Value::Integer_Type;
    callData->argc = 0;
    callData->thisObject = Value::fromObject(eng->globalObject);
    global = 0;
}

void WithContext::initWithContext(ExecutionContext *p, Object *with)
{
    initBaseContext(Type_WithContext, p->engine, p);
    callData = p->callData;
    outer = p;
    lookups = p->lookups;
    compilationUnit = p->compilationUnit;

    withObject = with;
}

void CatchContext::initCatchContext(ExecutionContext *p, String *exceptionVarName, const Value &exceptionValue)
{
    initBaseContext(Type_CatchContext, p->engine, p);
    strictMode = p->strictMode;
    callData = p->callData;
    outer = p;
    lookups = p->lookups;
    compilationUnit = p->compilationUnit;

    this->exceptionVarName = exceptionVarName;
    this->exceptionValue = exceptionValue;
}

void CallContext::initQmlContext(ExecutionContext *parentContext, Object *qml, FunctionObject *function)
{
    initBaseContext(Type_QmlContext, parentContext->engine, parentContext);

    this->function = function;
    this->callData = reinterpret_cast<CallData *>(this + 1);
    this->callData->tag = QV4::Value::Integer_Type;
    this->callData->argc = 0;
    this->callData->thisObject = Primitive::undefinedValue();

    strictMode = true;
    marked = false;
    this->outer = function->scope;
#ifndef QT_NO_DEBUG
    assert(outer->next != (ExecutionContext *)0x1);
#endif

    activation = qml;

    if (function->function) {
        compilationUnit = function->function->compilationUnit;
        lookups = compilationUnit->runtimeLookups;
    }

    locals = (Value *)(this + 1);
    if (function->varCount)
        std::fill(locals, locals + function->varCount, Primitive::undefinedValue());
}


bool ExecutionContext::deleteProperty(const StringRef name)
{
    Scope scope(this);
    bool hasWith = false;
    for (ExecutionContext *ctx = this; ctx; ctx = ctx->outer) {
        if (ctx->type == Type_WithContext) {
            hasWith = true;
            WithContext *w = static_cast<WithContext *>(ctx);
            if (w->withObject->__hasProperty__(name))
                return w->withObject->deleteProperty(name);
        } else if (ctx->type == Type_CatchContext) {
            CatchContext *c = static_cast<CatchContext *>(ctx);
            if (c->exceptionVarName->isEqualTo(name))
                return false;
        } else if (ctx->type >= Type_CallContext) {
            CallContext *c = static_cast<CallContext *>(ctx);
            FunctionObject *f = c->function;
            if (f->needsActivation || hasWith) {
                for (unsigned int i = 0; i < f->varCount; ++i)
                    if (f->varList[i]->isEqualTo(name))
                        return false;
                for (int i = (int)f->formalParameterCount - 1; i >= 0; --i)
                    if (f->formalParameterList[i]->isEqualTo(name))
                        return false;
            }
            if (c->activation && c->activation->__hasProperty__(name))
                return c->activation->deleteProperty(name);
        } else if (ctx->type == Type_GlobalContext) {
            GlobalContext *g = static_cast<GlobalContext *>(ctx);
            if (g->global->__hasProperty__(name))
                return g->global->deleteProperty(name);
        }
    }

    if (strictMode)
        throwSyntaxError(QString("Can't delete property %1").arg(name->toQString()));
    return true;
}

bool CallContext::needsOwnArguments() const
{
    return function->needsActivation || callData->argc < function->formalParameterCount;
}

void ExecutionContext::mark()
{
    if (marked)
        return;
    marked = true;

    if (type != Type_SimpleCallContext && outer)
        outer->mark();

    callData->thisObject.mark();
    for (unsigned arg = 0; arg < callData->argc; ++arg)
        callData->args[arg].mark();

    if (type >= Type_CallContext) {
        QV4::CallContext *c = static_cast<CallContext *>(this);
        for (unsigned local = 0, lastLocal = c->variableCount(); local < lastLocal; ++local)
            c->locals[local].mark();
        if (c->activation)
            c->activation->mark();
        c->function->mark();
    } else if (type == Type_WithContext) {
        WithContext *w = static_cast<WithContext *>(this);
        w->withObject->mark();
    } else if (type == Type_CatchContext) {
        CatchContext *c = static_cast<CatchContext *>(this);
        if (c->exceptionVarName)
            c->exceptionVarName->mark();
        c->exceptionValue.mark();
    } else if (type == Type_GlobalContext) {
        GlobalContext *g = static_cast<GlobalContext *>(this);
        g->global->mark();
    }
}

void ExecutionContext::setProperty(const StringRef name, const ValueRef value)
{
    Scope scope(this);
    for (ExecutionContext *ctx = this; ctx; ctx = ctx->outer) {
        if (ctx->type == Type_WithContext) {
            Object *w = static_cast<WithContext *>(ctx)->withObject;
            if (w->__hasProperty__(name)) {
                w->put(name, value);
                return;
            }
        } else if (ctx->type == Type_CatchContext && static_cast<CatchContext *>(ctx)->exceptionVarName->isEqualTo(name)) {
            static_cast<CatchContext *>(ctx)->exceptionValue = *value;
            return;
        } else {
            Object *activation = 0;
            if (ctx->type >= Type_CallContext) {
                CallContext *c = static_cast<CallContext *>(ctx);
                for (unsigned int i = 0; i < c->function->varCount; ++i)
                    if (c->function->varList[i]->isEqualTo(name)) {
                        c->locals[i] = *value;
                        return;
                    }
                for (int i = (int)c->function->formalParameterCount - 1; i >= 0; --i)
                    if (c->function->formalParameterList[i]->isEqualTo(name)) {
                        c->callData->args[i] = *value;
                        return;
                    }
                activation = c->activation;
            } else if (ctx->type == Type_GlobalContext) {
                activation = static_cast<GlobalContext *>(ctx)->global;
            }

            if (activation && (ctx->type == Type_QmlContext || activation->__hasProperty__(name))) {
                activation->put(name, value);
                return;
            }
        }
    }
    if (strictMode || name->isEqualTo(engine->id_this)) {
        ScopedValue n(scope, name.asReturnedValue());
        throwReferenceError(n);
    }
    engine->globalObject->put(name, value);
}

ReturnedValue ExecutionContext::getProperty(const StringRef name)
{
    Scope scope(this);
    ScopedValue v(scope);
    name->makeIdentifier();

    if (name->isEqualTo(engine->id_this))
        return callData->thisObject.asReturnedValue();

    bool hasWith = false;
    bool hasCatchScope = false;
    for (ExecutionContext *ctx = this; ctx; ctx = ctx->outer) {
        if (ctx->type == Type_WithContext) {
            Object *w = static_cast<WithContext *>(ctx)->withObject;
            hasWith = true;
            bool hasProperty = false;
            v = w->get(name, &hasProperty);
            if (hasProperty) {
                return v.asReturnedValue();
            }
            continue;
        }

        else if (ctx->type == Type_CatchContext) {
            hasCatchScope = true;
            CatchContext *c = static_cast<CatchContext *>(ctx);
            if (c->exceptionVarName->isEqualTo(name))
                return c->exceptionValue.asReturnedValue();
        }

        else if (ctx->type >= Type_CallContext) {
            QV4::CallContext *c = static_cast<CallContext *>(ctx);
            ScopedFunctionObject f(scope, c->function);
            if (f->needsActivation || hasWith || hasCatchScope) {
                for (unsigned int i = 0; i < f->varCount; ++i)
                    if (f->varList[i]->isEqualTo(name))
                        return c->locals[i].asReturnedValue();
                for (int i = (int)f->formalParameterCount - 1; i >= 0; --i)
                    if (f->formalParameterList[i]->isEqualTo(name))
                        return c->callData->args[i].asReturnedValue();
            }
            if (c->activation) {
                bool hasProperty = false;
                v = c->activation->get(name, &hasProperty);
                if (hasProperty)
                    return v.asReturnedValue();
            }
            if (f->function && f->function->isNamedExpression()
                && name->isEqualTo(f->function->name))
                return f.asReturnedValue();
        }

        else if (ctx->type == Type_GlobalContext) {
            GlobalContext *g = static_cast<GlobalContext *>(ctx);
            bool hasProperty = false;
            v = g->global->get(name, &hasProperty);
            if (hasProperty)
                return v.asReturnedValue();
        }
    }
    ScopedValue n(scope, name.asReturnedValue());
    throwReferenceError(n);
    return 0;
}

ReturnedValue ExecutionContext::getPropertyNoThrow(const StringRef name)
{
    Scope scope(this);
    ScopedValue v(scope);
    name->makeIdentifier();

    if (name->isEqualTo(engine->id_this))
        return callData->thisObject.asReturnedValue();

    bool hasWith = false;
    bool hasCatchScope = false;
    for (ExecutionContext *ctx = this; ctx; ctx = ctx->outer) {
        if (ctx->type == Type_WithContext) {
            Object *w = static_cast<WithContext *>(ctx)->withObject;
            hasWith = true;
            bool hasProperty = false;
            v = w->get(name, &hasProperty);
            if (hasProperty) {
                return v.asReturnedValue();
            }
            continue;
        }

        else if (ctx->type == Type_CatchContext) {
            hasCatchScope = true;
            CatchContext *c = static_cast<CatchContext *>(ctx);
            if (c->exceptionVarName->isEqualTo(name))
                return c->exceptionValue.asReturnedValue();
        }

        else if (ctx->type >= Type_CallContext) {
            QV4::CallContext *c = static_cast<CallContext *>(ctx);
            ScopedFunctionObject f(scope, c->function);
            if (f->needsActivation || hasWith || hasCatchScope) {
                for (unsigned int i = 0; i < f->varCount; ++i)
                    if (f->varList[i]->isEqualTo(name))
                        return c->locals[i].asReturnedValue();
                for (int i = (int)f->formalParameterCount - 1; i >= 0; --i)
                    if (f->formalParameterList[i]->isEqualTo(name))
                        return c->callData->args[i].asReturnedValue();
            }
            if (c->activation) {
                bool hasProperty = false;
                v = c->activation->get(name, &hasProperty);
                if (hasProperty)
                    return v.asReturnedValue();
            }
            if (f->function && f->function->isNamedExpression()
                && name->isEqualTo(f->function->name))
                return f.asReturnedValue();
        }

        else if (ctx->type == Type_GlobalContext) {
            GlobalContext *g = static_cast<GlobalContext *>(ctx);
            bool hasProperty = false;
            v = g->global->get(name, &hasProperty);
            if (hasProperty)
                return v.asReturnedValue();
        }
    }
    return Primitive::undefinedValue().asReturnedValue();
}

ReturnedValue ExecutionContext::getPropertyAndBase(const StringRef name, Object **base)
{
    Scope scope(this);
    ScopedValue v(scope);
    *base = 0;
    name->makeIdentifier();

    if (name->isEqualTo(engine->id_this))
        return callData->thisObject.asReturnedValue();

    bool hasWith = false;
    bool hasCatchScope = false;
    for (ExecutionContext *ctx = this; ctx; ctx = ctx->outer) {
        if (ctx->type == Type_WithContext) {
            Object *w = static_cast<WithContext *>(ctx)->withObject;
            hasWith = true;
            bool hasProperty = false;
            v = w->get(name, &hasProperty);
            if (hasProperty) {
                *base = w;
                return v.asReturnedValue();
            }
            continue;
        }

        else if (ctx->type == Type_CatchContext) {
            hasCatchScope = true;
            CatchContext *c = static_cast<CatchContext *>(ctx);
            if (c->exceptionVarName->isEqualTo(name))
                return c->exceptionValue.asReturnedValue();
        }

        else if (ctx->type >= Type_CallContext) {
            QV4::CallContext *c = static_cast<CallContext *>(ctx);
            FunctionObject *f = c->function;
            if (f->needsActivation || hasWith || hasCatchScope) {
                for (unsigned int i = 0; i < f->varCount; ++i)
                    if (f->varList[i]->isEqualTo(name))
                        return c->locals[i].asReturnedValue();
                for (int i = (int)f->formalParameterCount - 1; i >= 0; --i)
                    if (f->formalParameterList[i]->isEqualTo(name))
                        return c->callData->args[i].asReturnedValue();
            }
            if (c->activation) {
                bool hasProperty = false;
                v = c->activation->get(name, &hasProperty);
                if (hasProperty) {
                    if (ctx->type == Type_QmlContext)
                        *base = c->activation;
                    return v.asReturnedValue();
                }
            }
            if (f->function && f->function->isNamedExpression()
                && name->isEqualTo(f->function->name))
                return Value::fromObject(c->function).asReturnedValue();
        }

        else if (ctx->type == Type_GlobalContext) {
            GlobalContext *g = static_cast<GlobalContext *>(ctx);
            bool hasProperty = false;
            v = g->global->get(name, &hasProperty);
            if (hasProperty)
                return v.asReturnedValue();
        }
    }
    ScopedValue n(scope, name.asReturnedValue());
    throwReferenceError(n);
    return 0;
}


void ExecutionContext::throwError(const ValueRef value)
{
    __qmljs_throw(this, value);
}

void ExecutionContext::throwError(const QString &message)
{
    Scope scope(this);
    ScopedValue v(scope, Value::fromString(this, message));
    v = engine->newErrorObject(v);
    throwError(v);
}

void ExecutionContext::throwSyntaxError(const QString &message, const QString &fileName, int line, int column)
{
    Scope scope(this);
    Scoped<Object> error(scope, engine->newSyntaxErrorObject(message, fileName, line, column));
    throwError(error);
}

void ExecutionContext::throwSyntaxError(const QString &message)
{
    Scope scope(this);
    Scoped<Object> error(scope, engine->newSyntaxErrorObject(message));
    throwError(error);
}

void ExecutionContext::throwTypeError()
{
    Scope scope(this);
    Scoped<Object> error(scope, engine->newTypeErrorObject(QStringLiteral("Type error")));
    throwError(error);
}

void ExecutionContext::throwTypeError(const QString &message)
{
    Scope scope(this);
    Scoped<Object> error(scope, engine->newTypeErrorObject(message));
    throwError(error);
}

void ExecutionContext::throwUnimplemented(const QString &message)
{
    Scope scope(this);
    ScopedValue v(scope, Value::fromString(this, QStringLiteral("Unimplemented ") + message));
    v = engine->newErrorObject(v);
    throwError(v);
}

void ExecutionContext::throwReferenceError(const ValueRef value)
{
    Scope scope(this);
    Scoped<String> s(scope, value->toString(this));
    QString msg = s->toQString() + QStringLiteral(" is not defined");
    Scoped<Object> error(scope, engine->newReferenceErrorObject(msg));
    throwError(error);
}

void ExecutionContext::throwReferenceError(const QString &message, const QString &fileName, int line, int column)
{
    Scope scope(this);
    QString msg = message + QStringLiteral(" is not defined");
    Scoped<Object> error(scope, engine->newReferenceErrorObject(msg, fileName, line, column));
    throwError(error);
}

void ExecutionContext::throwRangeError(Value value)
{
    Scope scope(this);
    Scoped<String> s(scope, value.toString(this));
    QString msg = s->toQString() + QStringLiteral(" out of range");
    Scoped<Object> error(scope, engine->newRangeErrorObject(msg));
    throwError(error);
}

void ExecutionContext::throwURIError(Value msg)
{
    Scope scope(this);
    Scoped<Object> error(scope, engine->newURIErrorObject(msg));
    throwError(error);
}

void SimpleCallContext::initSimpleCallContext(ExecutionEngine *engine)
{
    initBaseContext(Type_SimpleCallContext, engine, engine->current);
    function = 0;
}
