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

#include <Common/Logger.h>
#include <string>
#include <memory>
#include <set>

#include <Core/UUID.h>
#include <Common/Exception.h>
#include <Common/HostWithPorts.h>
#include <Common/ThreadPool.h>
// #include <Bytepond/core/mapping/BpQueryKey.h>
// #include <Bytepond/core/storage/BlockHdfsStorageManager.h>
#include <Interpreters/StorageID.h>
#include <Interpreters/Context_fwd.h>
#include <Protos/cnch_common.pb.h>
#include <Protos/data_models.pb.h>
#include <Protos/cnch_worker_rpc.pb.h>

#include <brpc/closure_guard.h>
#include <brpc/controller.h>
#include <Poco/Logger.h>
#include <cppkafka/cppkafka.h>

namespace google::protobuf
{
class RpcController;
}

namespace brpc
{
class Controller;
}

namespace DB::RPCHelpers
{
    inline UUID createUUID(const Protos::UUID & uuid) { return UUID(UInt128{uuid.low(), uuid.high()}); }
    inline void fillUUID(UUID uuid, Protos::UUID & pb_uuid)
    {
        pb_uuid.set_low(uuid.toUnderType().items[0]);
        pb_uuid.set_high(uuid.toUnderType().items[1]);
    }

    inline StorageID createStorageID(const Protos::StorageID & id)
    {
        auto storage_id = StorageID(id.database(), id.table(), createUUID(id.uuid()));
        if (id.has_server_vw_name())
            storage_id.server_vw_name = id.server_vw_name();
        return storage_id;
    }
    inline void fillStorageID(const StorageID & id, Protos::StorageID & pb_id)
    {
        pb_id.set_database(id.database_name);
        pb_id.set_table(id.table_name);
        fillUUID(id.uuid, *pb_id.mutable_uuid());
        if (id.server_vw_name != DEFAULT_SERVER_VW_NAME)
            pb_id.set_server_vw_name(id.server_vw_name);
    }
    inline StorageID createStorageID(const Protos::DataModelTable & table)
    {
        auto storage_id = StorageID(table.database(), table.name(), createUUID(table.uuid()));
        if (table.has_server_vw_name())
            storage_id.server_vw_name = table.server_vw_name();
        return storage_id;
    }

    inline HostWithPorts createHostWithPorts(const Protos::HostWithPorts & hp)
    {
        return HostWithPorts{
            hp.host(),
            uint16_t(hp.rpc_port()),
            uint16_t(hp.tcp_port()),
            (hp.has_http_port() ? uint16_t(hp.http_port()) : uint16_t(0)),
            (hp.has_exchange_port() ? uint16_t(hp.exchange_port()) : uint16_t(0)),
            (hp.has_exchange_status_port() ? uint16_t(hp.exchange_status_port()) : uint16_t(0)),
            hp.hostname(),
        };
    }


    // inline BpQueryKeyPtr createBpQueryKey(const Protos::BpQueryKey & bqk)
    // {
    //     return shared_ptr<BpQueryKey>(new BpQueryKey(
    //         bqk.query(),
    //         bqk.settings_string(),
    //         bqk.stage(),
    //         bqk.concat_cols()
    //     ));
    // }

    // inline BpObjIdentifierPtr createBpObjIdentifier(const Protos::BpObjIdentifier & hi)
    // {
    //     return shared_ptr<BpObjIdentifier>(new BpObjIdentifier(
    //         hi.path(),
    //         hi.estimated_size(),
    //         hi.aio_threshold(),
    //         hi.mode()
    //     ));
    // }

    inline void fillHostWithPorts(const HostWithPorts & hp, Protos::HostWithPorts & pb_hp)
    {
        pb_hp.set_hostname(hp.id);
        pb_hp.set_host(hp.getHost());
        pb_hp.set_rpc_port(hp.rpc_port);
        pb_hp.set_tcp_port(hp.tcp_port);
        pb_hp.set_http_port(hp.http_port);
        pb_hp.set_exchange_port(hp.exchange_port);
        pb_hp.set_exchange_status_port(hp.exchange_status_port);
    }

    // inline void fillBpQueryKey(BpQueryKeyPtr query_key, Protos::BpQueryKey & pb_bqk)
    // {
    //     pb_bqk.set_query(query_key->getQuery());
    //     pb_bqk.set_settings_string(query_key->getSettingsString());
    //     pb_bqk.set_stage(query_key->getStage());
    //     pb_bqk.set_concat_cols(query_key->getConcatCols());
    // }

    // inline void fillBpObjIdentifier(BpObjIdentifierPtr id, Protos::BpObjIdentifier & pb_hi)
    // {
    //     pb_hi.set_path(id->getPath());
    //     pb_hi.set_estimated_size(id->getEstimatedSize());
    //     pb_hi.set_aio_threshold(id->getAioThreshold());
    //     pb_hi.set_mode(id->getMode());
    // }

    void handleException(std::string * exception_str);
    [[noreturn]] void checkException(const std::string & exception_str);

    template <class R>
    inline void checkResponse(const R & r)
    {
        if (r.has_exception())
            checkException(r.exception());
    }

    inline void fillKafkaTPL(const cppkafka::TopicPartitionList & tpl_, google::protobuf::RepeatedPtrField<Protos::TopicPartitionModel> & tpl_model)
    {
        for (auto & tp : tpl_)
        {
            auto * cur_tp = tpl_model.Add();
            cur_tp->set_topic(tp.get_topic());
            cur_tp->set_partition(tp.get_partition());
            cur_tp->set_offset(tp.get_offset());
        }
    }

    inline void createKafkaTPL(cppkafka::TopicPartitionList & tpl_, const google::protobuf::RepeatedPtrField<Protos::TopicPartitionModel> & tpl_model)
    {
        tpl_.reserve(tpl_model.size());
        for (const auto & tp : tpl_model)
            tpl_.emplace_back(cppkafka::TopicPartition(tp.topic(), tp.partition(), tp.offset()));
    }

    ContextMutablePtr createSessionContextForRPC(const ContextPtr & context, google::protobuf::RpcController & cntl_base);

    /// throw exception when cntl.Failed
    void assertController(const brpc::Controller & cntl);

    template<class Res>
    bool rpcFailed(const brpc::Controller & cntl, const Res & res)
    {
        return cntl.Failed() || res.has_exception();
    }

    template <typename Resp>
    void onAsyncCallDone(Resp * response, brpc::Controller * cntl, ExceptionHandlerPtr handler)
    {
        try
        {
            std::unique_ptr<Resp> response_guard(response);
            std::unique_ptr<brpc::Controller> cntl_guard(cntl);
            if (handler->hasException())
                return;
            if (rpcFailed(*cntl, *response) && handler->testException())
            {
                RPCHelpers::assertController(*cntl);
                RPCHelpers::checkResponse(*response);
            }
        }
        catch (...)
        {
            handler->setException(std::current_exception());
        }
    }

    template <typename Req, typename Resp>
    void onAsyncCallDoneAssertController(
        Req * request, Resp * response, brpc::Controller * cntl, LoggerPtr logger, std::function<String()> construct_err_msg)
    {
        // request will be use in construct_err_msg
        std::unique_ptr<Req> request_guard(request);
        try
        {
            std::unique_ptr<Resp> response_guard(response);
            std::unique_ptr<brpc::Controller> cntl_guard(cntl);
            RPCHelpers::assertController(*cntl);
        }
        catch (...)
        {
            tryLogCurrentException(logger, construct_err_msg());
        }
    }

    template <typename Resp, typename Handler>
    void onAsyncCallDoneWithFailedInfo(Resp * response, brpc::Controller * cntl, Handler handler, const DB::WorkerId worker_id)
    {
        int32_t error_code = 0;
        try
        {
            std::unique_ptr<Resp> response_guard(response);
            std::unique_ptr<brpc::Controller> cntl_guard(cntl);
            handler->addWorker(worker_id);
            auto failed = rpcFailed(*cntl, *response);
            if (failed)
            {
                error_code = cntl->ErrorCode();
                handler->addFailedRpc(worker_id, error_code);
            }
            if (handler->hasException())
                return;
            if (failed && handler->testException())
            {
                RPCHelpers::assertController(*cntl);
                RPCHelpers::checkResponse(*response);
            }
        }
        catch (...)
        {
            handler->setException(std::current_exception());
        }
    }

    template <typename Resp, typename Func>
    void serviceHandler(google::protobuf::Closure * done, Resp * resp, Func && f)
    {
        brpc::ClosureGuard done_guard(done);

        try
        {
            ThreadFromGlobalPool([func = std::forward<Func>(f)] { func(); }).detach();
            done_guard.release();
        }
        catch (...)
        {
            tryLogCurrentException(__PRETTY_FUNCTION__);
            RPCHelpers::handleException(resp->mutable_exception());
        }
    }

    inline WGWorkerInfoPtr createWorkerInfo(const Protos::WorkerInfo & worekr_info_data)
    {
        return std::make_shared<WGWorkerInfo>(worekr_info_data.worker_id(), worekr_info_data.num_workers(), worekr_info_data.index());
    }
}
