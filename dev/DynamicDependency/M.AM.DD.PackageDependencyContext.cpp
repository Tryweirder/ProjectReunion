﻿// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"

#include "M.AM.DD.PackageDependencyContext.h"
#include "Microsoft.ApplicationModel.DynamicDependency.PackageDependencyContext.g.cpp"

#include <M.AM.Converters.h>

namespace winrt::Microsoft::ApplicationModel::DynamicDependency::implementation
{
    PackageDependencyContext::PackageDependencyContext(Microsoft::ApplicationModel::DynamicDependency::PackageDependencyContextId const& contextId) :
        m_contextId(contextId)
    {
    }
    PackageDependencyContext::PackageDependencyContext(MDD_PACKAGEDEPENDENCY_CONTEXT context) :
        m_contextId(::Microsoft::ApplicationModel::DynamicDependency::ToContextId(context))
    {
    }
    winrt::PackageDependencyContextId PackageDependencyContext::ContextId()
    {
        return m_contextId;
    }

    hstring PackageDependencyContext::PackageFullName()
    {
        //TODO Lookup ContextId -> PackageFullName
        throw hresult_not_implemented();
    }

    void PackageDependencyContext::Remove()
    {
        MddRemovePackageDependency(::Microsoft::ApplicationModel::DynamicDependency::ToContext(m_contextId));
    }
}