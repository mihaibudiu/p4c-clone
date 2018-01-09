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

#ifndef _BACKENDS_EBPF_EBPFTYPE_H_
#define _BACKENDS_EBPF_EBPFTYPE_H_

#include "lib/algorithm.h"
#include "lib/sourceCodeBuilder.h"
#include "ebpfObject.h"
#include "ir/ir.h"

namespace EBPF {

/// Base class for EBPF types
class EBPFType : public EBPFObject {
 protected:
    explicit EBPFType(const IR::Type* type) : type(type) {}
 public:
    /// Actual P4 type that is represented.
    const IR::Type* type;
    /// Emits a C representation of a type declaration corresponding to this type.
    virtual void emit(CodeBuilder* builder) = 0;
    /// Emits a declaration of a C variable with this type.
    /// @param id  Name of the declared variable.
    virtual void declare(CodeBuilder* builder, cstring id) = 0;
    /// Emits C code to initialize a value of this type.
    virtual void emitInitializer(CodeBuilder* builder) = 0;
    virtual void declareArray(CodeBuilder* /*builder*/, cstring /*id*/, unsigned /*size*/)
    { BUG("%1%: unsupported array", type); }
    template<typename T> bool is() const { return dynamic_cast<const T*>(this) != nullptr; }
    template<typename T> T *to() { return dynamic_cast<T*>(this); }
};

/// Interface implemented by some EBPF types that have a statically-known width.
class IHasWidth {
 public:
    virtual ~IHasWidth() {}
    /// P4 width
    virtual unsigned widthInBits() const = 0;
    /// Width in the target implementation.
    virtual unsigned implementationWidthInBits() const = 0;
};

/// Type factory: converts IR::Type objects to EBPFType objects.
class EBPFTypeFactory {
 protected:
    const P4::TypeMap* typeMap;
    explicit EBPFTypeFactory(const P4::TypeMap* typeMap) :
            typeMap(typeMap) { CHECK_NULL(typeMap); }
 public:
    // Singleton pattern.
    static EBPFTypeFactory* instance;
    static void createFactory(const P4::TypeMap* typeMap)
    { EBPFTypeFactory::instance = new EBPFTypeFactory(typeMap); }
    /// Return the EBPF representation corresponding to this type.
    virtual EBPFType* create(const IR::Type* type);
};

/// EBPF type corresponding to IR::Type_Boolean
class EBPFBoolType : public EBPFType, public IHasWidth {
 public:
    EBPFBoolType() : EBPFType(IR::Type_Boolean::get()) {}
    void emit(CodeBuilder* builder) override
    { builder->append("u8"); }
    void declare(CodeBuilder* builder, cstring id) override;
    void emitInitializer(CodeBuilder* builder) override
    { builder->append("0"); }
    unsigned widthInBits() const override { return 1; }
    unsigned implementationWidthInBits() const override { return 8; }
};

/// Corresponds to an IR::Type_Stack
class EBPFStackType : public EBPFType, public IHasWidth {
    EBPFType* elementType;
    unsigned  size;
 public:
    EBPFStackType(const IR::Type_Stack* type, EBPFType* elementType) :
            EBPFType(type), elementType(elementType), size(type->getSize()) {
        CHECK_NULL(type); CHECK_NULL(elementType);
        BUG_CHECK(elementType->is<IHasWidth>(), "Unexpected element type %1%", elementType);
    }
    void emit(CodeBuilder*) override {}
    void declare(CodeBuilder* builder, cstring id) override;
    void emitInitializer(CodeBuilder* builder) override;
    unsigned widthInBits() const override;
    unsigned implementationWidthInBits() const override;
};

/// EBPF type corresponding to a simple scalar type (IR::Type_Bit).
class EBPFScalarType : public EBPFType, public IHasWidth {
 public:
    const unsigned width;
    const bool     isSigned;
    explicit EBPFScalarType(const IR::Type_Bits* bits) :
            EBPFType(bits), width(bits->size), isSigned(bits->isSigned) {}
    unsigned bytesRequired() const { return ROUNDUP(width, 8); }
    unsigned alignment() const;
    void emit(CodeBuilder* builder) override;
    void declare(CodeBuilder* builder, cstring id) override;
    void emitInitializer(CodeBuilder* builder) override
    { builder->append("0"); }
    unsigned widthInBits() const override { return width; }
    unsigned implementationWidthInBits() const  override { return bytesRequired() * 8; }
    // True if this width is small enough to store in a machine scalar
    static bool generatesScalar(unsigned width)
    { return width <= 64; }
};

/// This may not always implement IHasWidth, but we still have the interface
class EBPFTypeName : public EBPFType, public IHasWidth {
    const IR::Type_Name* type;
    EBPFType* canonical;
 public:
    EBPFTypeName(const IR::Type_Name* type, EBPFType* canonical) :
            EBPFType(type), type(type), canonical(canonical) {}
    void emit(CodeBuilder* builder) override { canonical->emit(builder); }
    void declare(CodeBuilder* builder, cstring id) override;
    void emitInitializer(CodeBuilder* builder) override;
    void declareArray(CodeBuilder* builder, cstring id, unsigned size) override;
    unsigned widthInBits() const override;
    unsigned implementationWidthInBits() const override;
};

/// Corresponds to a IR::Type_Header
class EBPFHeaderType : public EBPFType, public IHasWidth {
    unsigned width;
    cstring  name;
 public:
    EBPFHeaderType(const IR::Type_Header* strct, unsigned width);
    void declare(CodeBuilder* builder, cstring id) override;
    void emitInitializer(CodeBuilder* builder) override;
    unsigned widthInBits() const override { return width; }
    unsigned implementationWidthInBits() const override { return width; }
    void emit(CodeBuilder* builder) override;
};

/// Corresponds to a IR::Type_Struct or IR::Type_Union
class EBPFStructType : public EBPFType, public IHasWidth {
    class EBPFField {
     public:
        cstring comment;
        EBPFType* type;
        const IR::StructField* field;

        EBPFField(EBPFType* type, const IR::StructField* field, cstring comment = nullptr) :
            comment(comment), type(type), field(field) {}
    };

 public:
    cstring  kind;
    cstring  name;
    std::vector<EBPFField*>  fields;
    unsigned width;
    unsigned implWidth;

    explicit EBPFStructType(const IR::Type_StructLike* strct);
    void declare(CodeBuilder* builder, cstring id) override;
    void emitInitializer(CodeBuilder* builder) override;
    unsigned widthInBits() const override { return width; }
    unsigned implementationWidthInBits() const override { return implWidth; }
    void emit(CodeBuilder* builder) override;
    void declareArray(CodeBuilder* builder, cstring id, unsigned size) override;
};

/// Corresponds to an IR::Type_Enum
class EBPFEnumType : public EBPFType, public EBPF::IHasWidth {
 public:
    explicit EBPFEnumType(const IR::Type_Enum* type) : EBPFType(type) {}
    void emit(CodeBuilder* builder) override;
    void declare(CodeBuilder* builder, cstring id) override;
    void emitInitializer(CodeBuilder* builder) override
    { builder->append("0"); }
    unsigned widthInBits() const override { return 32; }
    unsigned implementationWidthInBits() const override { return 32; }

    const IR::Type_Enum* getType() const { return type->to<IR::Type_Enum>(); }
};

}  // namespace EBPF

#endif /* _BACKENDS_EBPF_EBPFTYPE_H_ */
