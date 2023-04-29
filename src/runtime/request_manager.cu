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

namespace FlexFlow {

using namespace Legion;

void RequestManager::load_tokens(Task const *task,
                                 std::vector<PhysicalRegion> const &regions,
                                 Context ctx,
                                 Runtime *runtime) {
  assert(regions.size() == 1);
  assert(task->regions.size() == 1);

  BatchConfig const batch_config = *((BatchConfig *)task->args);
  TokenId dram_copy[BatchConfig::MAX_NUM_TOKENS];
  for (int i = 0; i < batch_config.num_tokens; i++) {
    dram_copy[i] = batch_config.tokens[i].token_id;
  }
  TokenId *fb_ptr = helperGetTensorPointerWO<TokenId>(
      regions[0], task->regions[0], FID_DATA, ctx, runtime);
  Domain domain = runtime->get_index_space_domain(
      ctx, task->regions[0].region.get_index_space());
  assert(batch_config.num_tokens <= domain.get_volume());
  cudaStream_t stream;
  checkCUDA(get_legion_stream(&stream));
  checkCUDA(cudaMemcpyAync(fb_ptr,
                           dram_copy,
                           sizeof(TokenId) * batch_config.num_tokens,
                           cudaMemcpyHostToDevice,
                           stream));
};
