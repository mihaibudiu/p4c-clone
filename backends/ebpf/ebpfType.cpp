/*
Copyright 2013-present Barefoot Networks, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "ebpfType.h"

namespace EBPF {

EBPFTypeFactory* EBPFTypeFactory::instance;

EBPFType* EBPFTypeFactory::create(const IR::Type* type) {
    CHECK_NULL(type);
    CHECK_NULL(typeMap);
    EBPFType* result = nullptr;
    if (type->is<IR::Type_Boolean>()) {
        result = new EBPFBoolType();
    } else if (auto bt = type->to<IR::Type_Bits>()) {
        result = new EBPFScalarType(bt);
    } else if (auto th = type->to<IR::Type_Header>()) {
        unsigned width = 0;
        for (auto f : th->fields) {
            auto ft = typeMap->getType(f, true);
            if (ft->is<IR::Type_Varbits>())
                ::error("%1%: varbit types not supported", f->type);
            else
                width += ft->width_bits();
        }
        if (width % 8 != 0)
            ::error("Structure %1% width is not an integral number of bytes", th);
        result = new EBPFHeaderType(th, width);
    } else if (auto ts = type->to<IR::Type_StructLike>()) {
        result = new EBPFStructType(ts);
    } else if (auto ttd = type->to<IR::Type_Typedef>()) {
        auto canon = typeMap->getTypeType(type, true);
        result = create(canon);
        if (result == nullptr)
            return nullptr;
        auto path = new IR::Path(ttd->name);
        result = new EBPFTypeName(new IR::Type_Name(path), result);
    } else if (auto tn = type->to<IR::Type_Name>()) {
        auto canon = typeMap->getTypeType(type, true);
        result = create(canon);
        result = new EBPFTypeName(tn, result);
    } else if (auto te = type->to<IR::Type_Enum>()) {
        result = new EBPFEnumType(te);
    } else if (auto ts = type->to<IR::Type_Stack>()) {
        auto et = create(ts->elementType);
        if (et == nullptr)
            return nullptr;
        result = new EBPFStackType(ts, et);
    } else {
        ::error("Type %1% not supported", type);
    }

    return result;
}

void
EBPFBoolType::declare(CodeBuilder* builder, cstring id) {
    emit(builder);
    builder->appendFormat(" %s", id.c_str());
}

/////////////////////////////////////////////////////////////

void EBPFStackType::declare(CodeBuilder* builder, cstring id) {
    elementType->declareArray(builder, id, size);
}

void EBPFStackType::emitInitializer(CodeBuilder* builder) {
    builder->append("{");
    for (unsigned i = 0; i < size; i++) {
        if (i > 0)
            builder->append(", ");
        elementType->emitInitializer(builder);
    }
    builder->append(" }");
}

unsigned EBPFStackType::widthInBits() const {
    return size * elementType->to<IHasWidth>()->widthInBits();
}

unsigned EBPFStackType::implementationWidthInBits() const {
    return size * elementType->to<IHasWidth>()->implementationWidthInBits();
}

/////////////////////////////////////////////////////////////

unsigned EBPFScalarType::alignment() const {
    if (width <= 8)
        return 1;
    else if (width <= 16)
        return 2;
    else if (width <= 32)
        return 4;
    else if (width <= 64)
        return 8;
    else
        // compiled as u8*
        return 1;
}

void EBPFScalarType::emit(CodeBuilder* builder) {
    auto prefix = isSigned ? "i" : "u";

    if (width <= 8)
        builder->appendFormat("%s8", prefix);
    else if (width <= 16)
        builder->appendFormat("%s16", prefix);
    else if (width <= 32)
        builder->appendFormat("%s32", prefix);
    else if (width <= 64)
        builder->appendFormat("%s64", prefix);
    else
        builder->appendFormat("u8*");
}

void
EBPFScalarType::declare(CodeBuilder* builder, cstring id) {
    if (EBPFScalarType::generatesScalar(width)) {
        emit(builder);
        builder->spc();
        builder->append(id);
    } else {
        builder->appendFormat("u8 %s[%d]", id.c_str(), bytesRequired());
    }
}

//////////////////////////////////////////////////////////

EBPFHeaderType::EBPFHeaderType(const IR::Type_Header* strct, unsigned width) :
        EBPFType(strct), width(width), name(strct->name.name) {}

void
EBPFHeaderType::declare(CodeBuilder* builder, cstring id) {
    const char* n = name.c_str();
    builder->appendFormat("struct %s %s", n, id.c_str());
}

void EBPFHeaderType::emitInitializer(CodeBuilder* builder) {
    builder->blockStart();
    builder->emitIndent();
    builder->appendLine(".data = { 0 },");
    builder->emitIndent();
    builder->appendLine(".valid = 0");
    builder->blockEnd(false);
}

void EBPFHeaderType::emit(CodeBuilder* builder) {
    builder->emitIndent();
    builder->append("struct");
    builder->spc();
    builder->append(name);
    builder->spc();
    builder->blockStart();

    builder->emitIndent();
    builder->appendFormat("char data[%d]", width / 8);
    builder->endOfStatement(true);

    builder->emitIndent();
    auto type = EBPFTypeFactory::instance->create(IR::Type_Boolean::get());
    type->declare(builder, "valid");
    builder->endOfStatement(true);

    builder->blockEnd(false);
    builder->endOfStatement(true);
}

//////////////////////////////////////////////////////////

EBPFStructType::EBPFStructType(const IR::Type_StructLike* strct) :
        EBPFType(strct) {
    if (strct->is<IR::Type_Struct>())
        kind = "struct";
    else if (strct->is<IR::Type_HeaderUnion>())
        kind = "union";
    else
        BUG("Unexpected struct type %1%", strct);
    name = strct->name.name;
    width = 0;
    implWidth = 0;

    for (auto f : strct->fields) {
        auto type = EBPFTypeFactory::instance->create(f->type);
        auto wt = dynamic_cast<IHasWidth*>(type);
        if (wt == nullptr) {
            ::error("EBPF: Unsupported type in struct: %s", f->type);
        } else {
            width += wt->widthInBits();
            implWidth += wt->implementationWidthInBits();
        }
        fields.push_back(new EBPFField(type, f));
    }
}

void
EBPFStructType::declare(CodeBuilder* builder, cstring id) {
    builder->append(kind);
    builder->appendFormat(" %s %s", name.c_str(), id.c_str());
}

void EBPFStructType::emitInitializer(CodeBuilder* builder) {
    builder->blockStart();
    if (type->is<IR::Type_Struct>() || type->is<IR::Type_HeaderUnion>()) {
        for (auto f : fields) {
            builder->emitIndent();
            builder->appendFormat(".%s = ", f->field->name.name);
            f->type->emitInitializer(builder);
            builder->append(",");
            builder->newline();
        }
    } else {
        BUG("Unexpected type %1%", type);
    }
    builder->blockEnd(false);
}

void EBPFStructType::emit(CodeBuilder* builder) {
    builder->emitIndent();
    builder->append(kind);
    builder->spc();
    builder->append(name);
    builder->spc();
    builder->blockStart();

    for (auto f : fields) {
        auto type = f->type;
        builder->emitIndent();

        type->declare(builder, f->field->name);
        builder->append("; ");
        builder->append("/* ");
        builder->append(type->type->toString());
        if (f->comment != nullptr) {
            builder->append(" ");
            builder->append(f->comment);
        }
        builder->append(" */");
        builder->newline();
    }

    builder->blockEnd(false);
    builder->endOfStatement(true);
}

void
EBPFStructType::declareArray(CodeBuilder* builder, cstring id, unsigned size) {
    builder->appendFormat("%s %s[%d]", name.c_str(), id.c_str(), size);
}

///////////////////////////////////////////////////////////////

void EBPFTypeName::declare(CodeBuilder* builder, cstring id) {
    if (canonical != nullptr)
        canonical->declare(builder, id);
}

void EBPFTypeName::emitInitializer(CodeBuilder* builder) {
    if (canonical != nullptr)
        canonical->emitInitializer(builder);
}

unsigned EBPFTypeName::widthInBits() const {
    auto wt = dynamic_cast<IHasWidth*>(canonical);
    if (wt == nullptr) {
        ::error("Type %1% does not have a fixed witdh", type);
        return 0;
    }
    return wt->widthInBits();
}

unsigned EBPFTypeName::implementationWidthInBits() const {
    auto wt = dynamic_cast<IHasWidth*>(canonical);
    if (wt == nullptr) {
        ::error("Type %1% does not have a fixed witdh", type);
        return 0;
    }
    return wt->implementationWidthInBits();
}

void
EBPFTypeName::declareArray(CodeBuilder* builder, cstring id, unsigned size) {
    declare(builder, id);
    builder->appendFormat("[%d]", size);
}

////////////////////////////////////////////////////////////////

void EBPFEnumType::declare(EBPF::CodeBuilder* builder, cstring id) {
    builder->append("enum ");
    builder->append(getType()->name);
    builder->append(" ");
    builder->append(id);
}

void EBPFEnumType::emit(EBPF::CodeBuilder* builder) {
    builder->append("enum ");
    auto et = getType();
    builder->append(et->name);
    builder->blockStart();
    for (auto m : et->members) {
        builder->append(m->name);
        builder->appendLine(",");
    }
    builder->blockEnd(true);
}

}  // namespace EBPF
