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

#ifndef FRONTENDS_P4_CLONER_H_
#define FRONTENDS_P4_CLONER_H_

#include "ir/ir.h"

namespace P4 {

/// This transform converts identical PathExpression or Member nodes in a DAG
/// into distinct nodes.
class CloneExpressions : public Transform {
 public:
    CloneExpressions() {
        visitDagOnce = false;
        setName("CloneExpressions");
    }
    const IR::Node *postorder(IR::PathExpression *path) override {
        path->path = path->path->clone();
        return path;
    }

    // Clone expressions of the form Member(TypeNameExpression)
    const IR::Node *postorder(IR::Member *member) override {
        if (member->expr->is<IR::TypeNameExpression>()) {
            return new IR::Member(member->expr->clone(), member->member);
        }
        return member;
    }

    template <typename T>
    const T *clone(const IR::Node *node) {
        return node->apply(*this)->to<T>();
    }
};

}  // namespace P4

#endif /* FRONTENDS_P4_CLONER_H_ */
