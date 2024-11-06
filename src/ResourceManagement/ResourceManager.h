/*
 * Copyright (2022) Bytedance Ltd. and/or its affiliates
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <Common/Config/ConfigProcessor.h>
#include <Common/Logger.h>
#include <Server/IServer.h>
#include <daemon/BaseDaemon.h>

namespace DB
{

constexpr static auto ResourceManagerVersion = "1.0.0";

class ResourceManager: public BaseDaemon, public IServer
{
public:
    using ServerApplication::run;

    Poco::Util::LayeredConfiguration & config() const override
    {
        return BaseDaemon::config();
    }

    Poco::Logger & logger() const override
    {
        return BaseDaemon::logger();
    }

    ContextMutablePtr context() const override
    {
        return global_context;
    }

    bool isCancelled() const override
    {
        return BaseDaemon::isCancelled();
    }

    ~ResourceManager() override = default;

    void defineOptions(Poco::Util::OptionSet & _options) override;

    void initialize(Poco::Util::Application &) override;

protected:
    int run() override;

    int main(const std::vector<std::string> & args) override;

private:
    ContextMutablePtr global_context;
};

}
