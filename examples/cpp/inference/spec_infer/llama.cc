/* Copyright 2023 CMU, Facebook, LANL, MIT, NVIDIA, and Stanford (alphabetical)
 *
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

#include "models/llama.h"
#include "flexflow/inference.h"
#include "flexflow/tokenizers.h"
#include <nlohmann/json.hpp>

using namespace Legion;

LegionRuntime::Logger::Category log_app("llama");

struct FilePaths {
  std::string weight1_file_path;
  std::string weight2_file_path;
  std::string weight3_file_path;
  std::string weight4_file_path;
  std::string prompt_file_path;
  std::string tokenizer_file_path;
};

void parse_input_args(char **argv, int argc, FilePaths &paths) {
  for (int i = 1; i < argc; i++) {
    // weights
    if (!strcmp(argv[i], "--weight1")) {
      paths.weight1_file_path = std::string(argv[++i]);
      continue;
    }
    // weights
    if (!strcmp(argv[i], "--weight2")) {
      paths.weight2_file_path = std::string(argv[++i]);
      continue;
    }
    // weights
    if (!strcmp(argv[i], "--weight3")) {
      paths.weight3_file_path = std::string(argv[++i]);
      continue;
    }
    // weights
    if (!strcmp(argv[i], "--weight4")) {
      paths.weight4_file_path = std::string(argv[++i]);
      continue;
    }
    // prompts
    if (!strcmp(argv[i], "--prompt")) {
      paths.prompt_file_path = std::string(argv[++i]);
      continue;
    }
    // tokenizer
    if (!strcmp(argv[i], "--tokenizer")) {
      paths.tokenizer_file_path = std::string(argv[++i]);
      continue;
    }
  }
}

void FlexFlow::top_level_task(Task const *task,
                              std::vector<PhysicalRegion> const &regions,
                              Context ctx,
                              Runtime *runtime) {
  FFConfig ffconfig;
  FilePaths file_paths;

  InputArgs const &command_args = HighLevelRuntime::get_input_args();
  char **argv = command_args.argv;
  int argc = command_args.argc;
  parse_input_args(argv, argc, file_paths);
  SentencePieceTokenizer tokenizer(file_paths.tokenizer_file_path);
  InferenceManager im(ffconfig, BatchConfig::MAX_NUM_TOKENS, 1);
  RequestManager rm(&tokenizer);
  int total_num_requests = 0;
  if (false) {
    using json = nlohmann::json;
    std::ifstream file_handle(file_paths.prompt_file_path);
    assert(file_handle.good() && "Prompt file does not exist.");
    json prompt_json = json::parse(file_handle,
                                   /*parser_callback_t */ nullptr,
                                   /*allow_exceptions */ true,
                                   /*ignore_comments */ true);
    for (auto &prompt : prompt_json) {
      std::string text = prompt.get<std::string>();
      printf("Prompt[%d]: %s\n", total_num_requests, text.c_str());
      total_num_requests++;
      rm.register_new_request(text, 128 /*max_sequence_length*/);
      if (total_num_requests == 10) {
        break;
      }
    }
  } else {
    std::string text = "I believe the meaning of life is";
    rm.register_new_request(text, 128 /*max_sequence_length*/);
    total_num_requests = 1;
  }

  FFModel beam_model(ffconfig), tree_model(ffconfig);
  LLAMA::create_llama_model(beam_model,
                            im,
                            "190m",
                            file_paths.weight1_file_path,
                            1,
                            BEAM_SEARCH_MODE);
  LLAMA::create_llama_model(tree_model,
                            im,
                            "7b",
                            file_paths.weight2_file_path,
                            ffconfig.workersPerNode * ffconfig.numNodes,
                            TREE_VERIFY_MODE);

  TreeVerifyBatchConfig tree_bc;
  BeamSearchBatchConfig beam_bc;
  InferenceResult tree_ir;

  while (rm.get_num_processed_requests() < total_num_requests) {
    int depth = 0;
    // Beam Search
    beam_bc = rm.prepare_next_batch_init(tree_bc, tree_ir);
    if (rm.get_num_processed_requests() >= total_num_requests) {
      break;
    }
    while (true) {
      depth = beam_bc.beamRequestsInfo[0].current_depth;
      FutureMap fm = im.inference(&beam_model, 0, beam_bc);
      assert(fm.get_future_map_domain().get_volume() == 1);
      Future future = fm.get_future(0);
      BeamInferenceResult beam_ir = future.get_result<BeamInferenceResult>();
      if (depth - 1 >= BeamSearchBatchConfig::MAX_BEAM_DEPTH) {
        break;
      } else {
        beam_bc = rm.prepare_next_batch_beam(beam_bc, beam_ir);
      }
    }
    // Token Tree Verification
    {
      tree_bc = rm.prepare_next_batch_verify(beam_bc);
      FutureMap fm = im.inference(&tree_model, 0, tree_bc);
      assert(fm.get_future_map_domain().get_volume() == 1);
      Future future = fm.get_future(0);
      tree_ir = future.get_result<InferenceResult>();
    }
  }

  // Execution fence
  {
    Future future = runtime->issue_execution_fence(ctx);
    future.get_void_result();
  }

  // float* data
  std::cout << "----------inference finished--------------" << std::endl;
}

void FlexFlow::register_custom_tasks() {}
