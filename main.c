/* Haris Forge v10 -- split build root.
   Reassembles the original single-file runtime as ONE translation unit via
   #include, in the exact original order. No function body, macro, or
   declaration was changed, reordered, or deleted during the split -- only
   file boundaries were introduced, and boundaries were chosen so that every
   #if/#ifdef/#ifndef block stays fully inside a single file.
*/
#include "split/00_prelude_includes_types.c"
#include "split/01_jit_baseline.c"
#include "split/02_jit_linrec_lexer_parser_compiler.c"
#include "split/03_natives_core_part1.c"
#include "split/04_sandbox_backends.c"
#include "split/05_sandbox_exec_helpers.c"
#include "split/06_json_ai_data_layer.c"
#include "split/07_dataframe_analytics.c"
#include "split/08_web_fast_cloud.c"
#include "split/09_web_site_builder_server.c"
#include "split/10_web_https_tls.c"
#include "split/11_web_http3_quic.c"
#include "split/12_webrtc.c"
#include "split/13_cuda_driver.c"
#include "split/14_mixed_precision.c"
#include "split/15_cnn_rnn_lstm.c"
#include "split/16_transformer.c"
#include "split/17_model_zoo.c"
#include "split/18_hf_hub.c"
#include "split/19_distributed_allreduce.c"
#include "split/20_onnx.c"
#include "split/21_graph_fusion.c"
#include "split/22_v23_ai_db_net_xss.c"
#include "split/23_extended_ai_game_sdk.c"
#include "split/24_stdlib_primitives.c"
#include "split/25_multiplayer_reliable_udp_and_v10_server.c"
#include "split/26_init_run_main.c"
