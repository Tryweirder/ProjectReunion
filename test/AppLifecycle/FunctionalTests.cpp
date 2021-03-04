﻿// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include <testdef.h>
#include "Shared.h"

namespace Test::FileSystem
{
    constexpr PCWSTR ThisTestDllFilename{ L"CppTest.dll" };
}
#include <ProjectReunion.Test.Bootstrap.h>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace winrt::Microsoft::ApplicationModel::Activation;
using namespace winrt;
using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Management::Deployment;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::System;

namespace TP = ::Test::Packages;

// TODO: Write Register/Unregister tests that utilize the Assoc APIs to validate results.

void DumpUser(PCWSTR context, PSID userSid)
{
    if (userSid)
    {
        wil::unique_hlocal_string userSidAsString;
        VERIFY_WIN32_BOOL_SUCCEEDED(ConvertSidToStringSidW(userSid, &userSidAsString));
        WEX::Logging::Log::Comment(WEX::Common::String().Format(L"UserSid:%s %s", context, userSidAsString.get()));
    }
    else
    {
        WEX::Logging::Log::Comment(WEX::Common::String().Format(L"UserSid:%s null", context));
    }
}

void DumpUser()
{
    {
        auto tokenUser{ wil::get_token_information<TOKEN_USER>(GetCurrentThreadEffectiveToken()) };
        PSID effectiveUserSid{ tokenUser->User.Sid };
        DumpUser(L"Effective", effectiveUserSid);
    }

    {
        auto tokenUser{ wil::get_token_information<TOKEN_USER>(GetCurrentProcessToken()) };
        PSID effectiveUserSid{ tokenUser->User.Sid };
        DumpUser(L"Process", effectiveUserSid);
    }

    const auto isAppContainer{ wil::get_token_is_app_container() };
    WEX::Logging::Log::Comment(WEX::Common::String().Format(L"IsAppContainer:%s", isAppContainer ? L"True" : L"False"));
}

namespace ProjectReunionCppTest
{
    class AppLifecycleFunctionalTests
    {
    private:
        wil::unique_event m_failed;

        const std::wstring c_testDataFileName = L"testfile" + c_testFileExtension;
        const std::wstring c_testDataFileName_Packaged = L"testfile" + c_testFileExtension_Packaged;
        const std::wstring c_testPackageFile = g_deploymentDir + L"AppLifecycleTestPackage.msixbundle";
        const std::wstring c_testPackageCertFile = g_deploymentDir + L"AppLifecycleTestPackage.cer";
        const std::wstring c_testPackageFullName = L"AppLifecycleTestPackage_1.0.0.0_x64__ph1m9x8skttmg";

    public:
        BEGIN_TEST_CLASS(AppLifecycleFunctionalTests)
            TEST_CLASS_PROPERTY(L"ThreadingModel", L"MTA")
            TEST_CLASS_PROPERTY(L"RunFixtureAs:Class", L"InteractiveUser")
        END_TEST_CLASS()

        TEST_CLASS_SETUP(ClassInit)
        {
            DumpUser();

            ::Test::Bootstrap::SetupPackages();

            // Deploy packaged app to register handler through the manifest.
            //RunCertUtil(c_testPackageCertFile);
            //InstallPackage(c_testPackageFile);

            // Write out some test content.
            WriteContentFile(c_testDataFileName);
            WriteContentFile(c_testDataFileName_Packaged);

            return true;
        }

        TEST_CLASS_CLEANUP(ClassUninit)
        {
            DumpUser();

            // Swallow errors in cleanup.
            try
            {
                DeleteContentFile(c_testDataFileName_Packaged);
                DeleteContentFile(c_testDataFileName);
                //UninstallPackage(c_testPackageFullName);
                //RunCertUtil(c_testPackageCertFile, true);
            }
            catch (const std::exception&)
            {
            }
            catch (const winrt::hresult_error&)
            {
            }

            ::Test::Bootstrap::CleanupPackages();
            return true;
        }

        TEST_METHOD_SETUP(MethodInit)
        {
            DumpUser();

            ::Test::Bootstrap::Setup();

            m_failed = CreateTestEvent(c_testFailureEventName);
            return true;
        }

        TEST_METHOD_CLEANUP(MethodShutdown)
        {
            DumpUser();

            ::Test::Bootstrap::Cleanup();
            return true;
        }

        TEST_METHOD(GetActivatedEventArgsIsNotNull)
        {
            BEGIN_TEST_METHOD_PROPERTIES()
                TEST_METHOD_PROPERTY(L"RunAs", L"InteractiveUser")
            END_TEST_METHOD_PROPERTIES();

            VERIFY_IS_NOT_NULL(AppLifecycle::GetActivatedEventArgs());
        }

        TEST_METHOD(GetActivatedEventArgsForLaunch)
        {
            BEGIN_TEST_METHOD_PROPERTIES()
                TEST_METHOD_PROPERTY(L"RunAs", L"InteractiveUser")
            END_TEST_METHOD_PROPERTIES();

            auto args = AppLifecycle::GetActivatedEventArgs();
            VERIFY_IS_NOT_NULL(args);
            VERIFY_ARE_EQUAL(args.Kind(), ActivationKind::Launch);

            auto launchArgs = args.as<LaunchActivatedEventArgs>();
            VERIFY_IS_NOT_NULL(launchArgs);
        }

        TEST_METHOD(GetActivatedEventArgsForProtocol_Win32)
        {
            // Create a named event for communicating with test app.
            auto event = CreateTestEvent(c_testProtocolPhaseEventName);

            // Launch the test app to register for protocol launches.
            Execute(L"AppLifecycleTestApp.exe", L"/RegisterProtocol", g_deploymentDir);

            // Wait for the register event.
            WaitForEvent(event, m_failed);

            // Launch a protocol and wait for the event to fire.
            Uri launchUri{ c_testProtocolScheme + L"://this_is_a_test" };
            auto launchResult = Launcher::LaunchUriAsync(launchUri).get();
            VERIFY_IS_TRUE(launchResult);

            // Wait for the protocol activation.
            WaitForEvent(event, m_failed);

            Execute(L"AppLifecycleTestApp.exe", L"/RegisterProtocol", g_deploymentDir);

            // Wait for the unregister event.
            WaitForEvent(event, m_failed);
        }

        TEST_METHOD(GetActivatedEventArgsForFile_Win32)
        {
            // Create a named event for communicating with test app.
            auto event = CreateTestEvent(c_testFilePhaseEventName);

            // Launch the test app to register for protocol launches.
            Execute(L"AppLifecycleTestApp.exe", L"/RegisterFile", g_deploymentDir);

            // Wait for the register event.
            WaitForEvent(event, m_failed);

            // Launch the file and wait for the event to fire.
            auto file = OpenDocFile(c_testDataFileName);
            auto launchResult = Launcher::LaunchFileAsync(file).get();
            VERIFY_IS_TRUE(launchResult);

            // Wait for the protocol activation.
            WaitForEvent(event, m_failed);
        }
    };

    class AppLifecycleUAPFunctionalTests
    {
    private:
        wil::unique_event m_failed;

        const std::wstring c_testDataFileName = L"testfile" + c_testFileExtension;
        const std::wstring c_testDataFileName_Packaged = L"testfile" + c_testFileExtension_Packaged;
        const std::wstring c_testPackageFile = g_deploymentDir + L"AppLifecycleTestPackage.msixbundle";
        const std::wstring c_testPackageCertFile = g_deploymentDir + L"AppLifecycleTestPackage.cer";
        const std::wstring c_testPackageFullName = L"AppLifecycleTestPackage_1.0.0.0_x64__ph1m9x8skttmg";

    public:
        BEGIN_TEST_CLASS(AppLifecycleUAPFunctionalTests)
            TEST_CLASS_PROPERTY(L"ThreadingModel", L"MTA")
            TEST_CLASS_PROPERTY(L"RunFixtureAs:Class", L"InteractiveUser")
        END_TEST_CLASS()

        TEST_CLASS_SETUP(ClassInit)
        {
            DumpUser();

            ::Test::Bootstrap::SetupPackages();

            // Write out some test content.
            WriteContentFile(c_testDataFileName);
            WriteContentFile(c_testDataFileName_Packaged);

            return true;
        }

        TEST_CLASS_CLEANUP(ClassUninit)
        {
            DumpUser();

            // Swallow errors in cleanup.
            try
            {
                DeleteContentFile(c_testDataFileName_Packaged);
                DeleteContentFile(c_testDataFileName);
            }
            catch (const std::exception&)
            {
            }
            catch (const winrt::hresult_error&)
            {
            }

            ::Test::Bootstrap::CleanupPackages();
            return true;
        }

        TEST_METHOD_SETUP(MethodInit)
        {
            DumpUser();

            //TP::AddPackage_ProjectReunionFramework();
            //TP::AddPackage_DynamicDependencyDataStore();
            //TP::AddPackage_DynamicDependencyLifetimeManager();
            VERIFY_IS_TRUE(TP::IsPackageRegistered_ProjectReunionFramework());
            VERIFY_IS_TRUE(TP::IsPackageRegistered_DynamicDependencyDataStore());
            VERIFY_IS_TRUE(TP::IsPackageRegistered_DynamicDependencyLifetimeManager());
            ::Test::Bootstrap::SetupBootstrap();

            m_failed = CreateTestEvent(c_testFailureEventName);
            return true;
        }

        TEST_METHOD_CLEANUP(MethodShutdown)
        {
            DumpUser();

            ::Test::Bootstrap::Cleanup();
            return true;
        }

        // Validate that UWP is not a supported scenario.
        TEST_METHOD(GetActivatedEventArgsIsNull)
        {
            BEGIN_TEST_METHOD_PROPERTIES()
                TEST_METHOD_PROPERTY(L"RunAs", L"UAP")
                TEST_METHOD_PROPERTY(L"UAP:AppxManifest", L"AppLifecycle-AppxManifest.xml")
            END_TEST_METHOD_PROPERTIES();

            VERIFY_IS_NULL(AppLifecycle::GetActivatedEventArgs());
        }
    };
}
