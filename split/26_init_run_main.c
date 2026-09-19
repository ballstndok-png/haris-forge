static void init(VM*vm){memset(vm,0,sizeof*vm);uint64_t seed=0;
    if(RAND_bytes((unsigned char*)&seed,sizeof(seed))!=1||seed==0)seed=(uint64_t)time(NULL)^(uint64_t)(uintptr_t)vm;
    hr_rng_seed(vm,seed);vm->cpu_percent=100;vm->cpu_affinity_ok=1;vm->int_overflow=0;vm->module_sandboxed=0;vm->module_caps=0;vm->host_module_caps=CAP_SAFE_DEFAULT;snprintf(vm->sandbox_backend,sizeof vm->sandbox_backend,"host");apply_auto_memory(vm);bindn(&vm->g,"print",nprint);bindn(&vm->g,"sandbox.run",sandbox_run_code);bindn(&vm->g,"sandbox.system",sandbox_system);bindn(&vm->g,"sandbox.read",sandbox_read);bindn(&vm->g,"sandbox.write",sandbox_write);bindn(&vm->g,"sandbox.info",sandbox_info);bindn(&vm->g,"len",nlen);bindn(&vm->g,"list.append",nappend);bindn(&vm->g,"list.push",nappend);bindn(&vm->g,"list.set",nlist_set);bindn(&vm->g,"list.pop",npop);bindn(&vm->g,"list.insert",ninsert);bindn(&vm->g,"string.substring",nsubstr);bindn(&vm->g,"string.replace",nreplace);bindn(&vm->g,"string.upper",nupper);bindn(&vm->g,"string.lower",nlower);bindn(&vm->g,"string.split",nsplit);bindn(&vm->g,"string.find",nstr_find);bindn(&vm->g,"string.starts_with",nstr_starts);bindn(&vm->g,"string.ends_with",nstr_ends);bindn(&vm->g,"string.trim",nstr_trim);bindn(&vm->g,"string.char_at",nstr_char_at);bindn(&vm->g,"string.ord",nstr_ord);bindn(&vm->g,"string.chr",nstr_chr);bindn(&vm->g,"string.format",nstr_format);bindn(&vm->g,"string.utf8_len",nstr_utf8_len);bindn(&vm->g,"string.utf8_at",nstr_utf8_at);bindn(&vm->g,"string.utf8_chr",nstr_utf8_chr);bindn(&vm->g,"math.abs",nabs);bindn(&vm->g,"math.overflowed",nmath_overflowed);bindn(&vm->g,"math.clear_overflow",nmath_clear_overflow);bindn(&vm->g,"random.int",hr_rand_int);bindn(&vm->g,"random.float",hr_rand_float);bindn(&vm->g,"random.int_range",hr_rand_int_range);bindn(&vm->g,"random.float_range",hr_rand_float_range);bindn(&vm->g,"math.deg_to_rad",hr_deg_to_rad);bindn(&vm->g,"math.rad_to_deg",hr_rad_to_deg);bindn(&vm->g,"math.sign",hr_sign);bindn(&vm->g,"math.min",hr_min);bindn(&vm->g,"math.max",hr_max);bindn(&vm->g,"math.sqrt",nsqrt);bindn(&vm->g,"math.pow",npow);bindn(&vm->g,"math.floor",nfloor);bindn(&vm->g,"math.ceil",nceil);bindn(&vm->g,"math.clamp",math_clamp2);bindn(&vm->g,"math.is_equal_approx",math_approx2);bindn(&vm->g,"__import",nimport);bindn(&vm->g,"os.getenv",nosenv);bindn(&vm->g,"os.exists",nosexists);bindn(&vm->g,"file.open",nfile_open);bindn(&vm->g,"file.read",nfile_read_any);bindn(&vm->g,"file.write",nfile_write_any);bindn(&vm->g,"file.close",nfile_close);bindn(&vm->g,"file.append",nfile_append);bindn(&vm->g,"dir.exists",ndir_exists);bindn(&vm->g,"dir.list",ndir_list);bindn(&vm->g,"path.join",npath_join);bindn(&vm->g,"path.basename",npath_basename);bindn(&vm->g,"path.dirname",npath_dirname);bindn(&vm->g,"path.extension",npath_extension);bindn(&vm->g,"fs.read",nfs_read);bindn(&vm->g,"fs.write",nfs_write);bindn(&vm->g,"fs.delete",nfs_delete);bindn(&vm->g,"fs.list_dir",nfs_list_dir);bindn(&vm->g,"fs.copy",nfs_copy);bindn(&vm->g,"fs.move",nfs_move);bindn(&vm->g,"map.new",nmap_new);bindn(&vm->g,"set.new",nset_new);bindn(&vm->g,"set.add",nset_add);bindn(&vm->g,"set.has",nset_has);bindn(&vm->g,"set.size",nset_size);bindn(&vm->g,"queue.new",nqueue_new);bindn(&vm->g,"queue.push",nqueue_push);bindn(&vm->g,"queue.pop",nqueue_pop);bindn(&vm->g,"stack.new",nstack_new);bindn(&vm->g,"stack.push",nstack_push);bindn(&vm->g,"stack.pop",nstack_pop);bindn(&vm->g,"stack.size",nstack_size);bindn(&vm->g,"map.get",nmap_get);bindn(&vm->g,"map.set",nmap_set);bindn(&vm->g,"map.has",nmap_has);bindn(&vm->g,"map.keys",nmap_keys);bindn(&vm->g,"map.items",nmap_items);bindn(&vm->g,"map.delete",nmap_delete);bindn(&vm->g,"map.remove",nmap_delete);bindn(&vm->g,"map.values",nmap_values);bindn(&vm->g,"map.size",nmap_size);bindn(&vm->g,"os.time_ms",nostime);bindn(&vm->g,"os.monotonic_ms",os_monotonic_ms);bindn(&vm->g,"int",nint_cast);bindn(&vm->g,"float",nfloat_cast);bindn(&vm->g,"system.run",nsystem);bindn(&vm->g,"net.ip",nip);bindn(&vm->g,"net.mac",nmac);bindn(&vm->g,"net.dns",ndns);bindn(&vm->g,"udp.open",udp_open);bindn(&vm->g,"udp.bind",udp_bind);bindn(&vm->g,"udp.send",udp_send);bindn(&vm->g,"udp.recv",udp_recv);bindn(&vm->g,"udp.send_batch",udp_send_batch);bindn(&vm->g,"udp.connect",udp_connect);bindn(&vm->g,"udp.set_nonblock",udp_set_nonblock);bindn(&vm->g,"udp.set_buffer",udp_set_buffer);bindn(&vm->g,"udp.close",udp_close);bindn(&vm->g,"udp.info",udp_info);bindn(&vm->g,"ws.connect",ws_connect);bindn(&vm->g,"ws.listen",ws_listen);bindn(&vm->g,"ws.accept",ws_accept);bindn(&vm->g,"ws.send_text",ws_send_text);bindn(&vm->g,"ws.send_binary",ws_send_binary);bindn(&vm->g,"ws.recv",ws_recv);bindn(&vm->g,"ws.ping",ws_ping);bindn(&vm->g,"ws.close",ws_close);bindn(&vm->g,"ws.info",ws_info);bindn(&vm->g,"net.connect_check",net_connect_check);bindn(&vm->g,"net.port_scan",net_port_scan);bindn(&vm->g,"net.banner_grab",net_banner_grab);bindn(&vm->g,"net.parse_host",net_parse_host);bindn(&vm->g,"net.status",net_status);bindn(&vm->g,"net.firewall_status",nfw);bindn(&vm->g,"net.http_get",nwebget);bindn(&vm->g,"net.http_post_json",nwebpost);bindn(&vm->g,"web.get",nwebget);bindn(&vm->g,"web.post_json",nwebpost);bindn(&vm->g,"web.request",web_request_fast);bindn(&vm->g,"web.secure_get",web_secure_get);bindn(&vm->g,"web.secure_pin",web_secure_pin);bindn(&vm->g,"web.tls_info",web_tls_info);bindn(&vm->g,"net.dns.resolve",net_dns_resolve);bindn(&vm->g,"net.dns.doh",net_dns_doh);bindn(&vm->g,"dns.resolve",net_dns_resolve);bindn(&vm->g,"dns.doh",net_dns_doh);bindn(&vm->g,"web.get_json",web_get_json);bindn(&vm->g,"web.batch",web_batch);bindn(&vm->g,"web.fast_batch",web_batch);bindn(&vm->g,"web.parallel",web_batch);bindn(&vm->g,"web.url_encode",web_url_encode);bindn(&vm->g,"web.url_decode",web_url_decode_value);bindn(&vm->g,"web.response_headers",web_request_headers);bindn(&vm->g,"web.download",web_download);bindn(&vm->g,"web.upload",web_upload);bindn(&vm->g,"web.security_headers",nwebheaders);bindn(&vm->g,"web.redirect_chain",web_redirect_chain);bindn(&vm->g,"web.dir_bruteforce",web_dir_bruteforce);bindn(&vm->g,"web.safe_url",nwebsafe);bindn(&vm->g,"web.html",web_html);bindn(&vm->g,"web.render",web_render);bindn(&vm->g,"web.build",web_build);bindn(&vm->g,"web.serve_static",web_serve_static);bindn(&vm->g,"web.https_serve_static",web_https_serve_static);bindn(&vm->g,"web.tls_profile",web_tls_profile);bindn(&vm->g,"web.http3_serve_static",web_http3_serve_static);bindn(&vm->g,"web.http3_info",web_http3_info);bindn(&vm->g,"webrtc.available",webrtc_available);bindn(&vm->g,"webrtc.peer",webrtc_peer);bindn(&vm->g,"webrtc.offer",webrtc_offer);bindn(&vm->g,"webrtc.answer",webrtc_answer);bindn(&vm->g,"webrtc.set_remote",webrtc_set_remote);bindn(&vm->g,"webrtc.add_candidate",webrtc_candidate);bindn(&vm->g,"webrtc.data_channel",webrtc_data_channel);bindn(&vm->g,"webrtc.send",webrtc_send);bindn(&vm->g,"webrtc.recv",webrtc_recv);bindn(&vm->g,"webrtc.info",webrtc_info);bindn(&vm->g,"webrtc.close",webrtc_close);bindn(&vm->g,"web.app",app_new);bindn(&vm->g,"web.app.https_listen",app_https_listen);bindn(&vm->g,"web.response",web_response);bindn(&vm->g,"cloud.get",nwebget);bindn(&vm->g,"cloud.post_json",nwebpost);bindn(&vm->g,"cloud.upload",nput);bindn(&vm->g,"cloud.download",cloud_download);bindn(&vm->g,"cloud.request",cloud_request);bindn(&vm->g,"cloud.put_json",cloud_put_json);bindn(&vm->g,"cloud.delete",cloud_delete);bindn(&vm->g,"cloud.env",cloud_env);bindn(&vm->g,"cloud.get_json",cloud_get_json);bindn(&vm->g,"cloud.post_json",cloud_post_json);bindn(&vm->g,"cloud.request_fast",cloud_request);bindn(&vm->g,"api.request",api_request);bindn(&vm->g,"api.get",api_get);bindn(&vm->g,"api.post_json",api_post_json);bindn(&vm->g,"api.put_json",api_put_json);bindn(&vm->g,"api.delete",api_delete);bindn(&vm->g,"api.json",api_json);bindn(&vm->g,"api.batch",api_batch);bindn(&vm->g,"cloud.kv_get",cloud_kv_get);bindn(&vm->g,"cloud.kv_set",cloud_kv_set);bindn(&vm->g,"cloud.storage_upload",cloud_storage_upload);bindn(&vm->g,"cloud.storage_download",cloud_storage_download);bindn(&vm->g,"sql.open",nsqlopen);bindn(&vm->g,"sql.exec",nsqlexec);bindn(&vm->g,"sql.query",nsqlquery);bindn(&vm->g,"sql.close",nsqlclose);bindn(&vm->g,"sql.prepare_bind",nsqlbind);bindn(&vm->g,"sql.tables",security_sqlite_tables);bindn(&vm->g,"sql.schema",security_sqlite_schema);bindn(&vm->g,"sql.begin",sql_begin);bindn(&vm->g,"sql.commit",sql_commit);bindn(&vm->g,"sql.rollback",sql_rollback);bindn(&vm->g,"sql.bind",sql_exec_bind);bindn(&vm->g,"sql.quote_identifier",sql_quote_identifier);bindn(&vm->g,"logs.read",nlogread);bindn(&vm->g,"logs.count",nlogcount);bindn(&vm->g,"logs.levels",nloglevels);bindn(&vm->g,"data.csv",ndata_csv);bindn(&vm->g,"data.read_csv",data_read_csv);bindn(&vm->g,"data.from_rows",data_from_rows);bindn(&vm->g,"data.rows",data_rows);bindn(&vm->g,"data.shape",data_shape);bindn(&vm->g,"data.column",data_column);bindn(&vm->g,"data.filter",data_filter);bindn(&vm->g,"data.sort",data_sort);bindn(&vm->g,"data.head",data_head);bindn(&vm->g,"data.tail",data_tail);bindn(&vm->g,"data.describe",data_describe);bindn(&vm->g,"data.groupby_count",data_group_count);bindn(&vm->g,"data.drop_missing",data_drop_missing);bindn(&vm->g,"data.fill_missing",data_fill_missing);bindn(&vm->g,"data.sample",data_sample);bindn(&vm->g,"data.to_csv",data_write_csv);bindn(&vm->g,"data.correlation",data_corr);bindn(&vm->g,"data.covariance",data_cov);bindn(&vm->g,"data.regression",data_regression);bindn(&vm->g,"data.zscore",data_zscore);bindn(&vm->g,"data.minmax",data_minmax);bindn(&vm->g,"data.read_jsonl",data_jsonl);bindn(&vm->g,"data.stats",nstats);bindn(&vm->g,"defense.log_levels",nloglevels);bindn(&vm->g,"git.version",ngitver);bindn(&vm->g,"git.status",ngitstatus);bindn(&vm->g,"git.log",ngitlog);bindn(&vm->g,"git.clone",ngitclone);bindn(&vm->g,"git.pull",ngitpull);bindn(&vm->g,"git.push",ngitpush);bindn(&vm->g,"git.add",ngitadd);bindn(&vm->g,"git.commit",ngitcommit);bindn(&vm->g,"git.diff",ngitdiff);bindn(&vm->g,"git.branch",ngitbranch);bindn(&vm->g,"git.checkout",ngitcheckout);bindn(&vm->g,"git.info",git_info);bindn(&vm->g,"git.is_clean",git_is_clean);bindn(&vm->g,"git.head",git_head);bindn(&vm->g,"git.status_files",git_status_files);bindn(&vm->g,"git.diff_stat",git_diff_stat);bindn(&vm->g,"git.stage_all",git_stage_all);bindn(&vm->g,"git.commit_if_dirty",git_commit_if_dirty);bindn(&vm->g,"git.create_branch",git_create_branch);bindn(&vm->g,"json.stringify",njson_stringify);bindn(&vm->g,"json.parse",njson_parse);bindn(&vm->g,"ai.system",nai_system);bindn(&vm->g,"ai.user",nai_user);bindn(&vm->g,"ai.assistant",nai_assistant);bindn(&vm->g,"ai.tool",nai_tool);bindn(&vm->g,"ai.messages",nai_messages);bindn(&vm->g,"ai.chat_messages",nai_chat_messages);bindn(&vm->g,"ai.request",nai_request);bindn(&vm->g,"ai.dot",nai_dot);bindn(&vm->g,"ai.cosine",nai_cosine);bindn(&vm->g,"ai.sigmoid",nai_sigmoid);bindn(&vm->g,"ai.argmax",nai_argmax);bindn(&vm->g,"ai.softmax",nai_softmax);bindn(&vm->g,"ai.linear",nai_linear);bindn(&vm->g,"ai.chat",nai_chat);bindn(&vm->g,"games.distance",ngdistance);bindn(&vm->g,"games.seek",ngseek);bindn(&vm->g,"games.flee",ngflee);bindn(&vm->g,"games.safe_path",ngsafe);bindn(&vm->g,"defense.web_headers",nwebheaders);bindn(&vm->g,"defense.tls_info",web_tls_info);bindn(&vm->g,"defense.safe_url",nwebsafe);bindn(&vm->g,"defense.log_count",nlogcount);bindn(&vm->g,"defense.path_safe",ngsafe);bindn(&vm->g,"ai.vec_add",nai_vec_add);bindn(&vm->g,"ai.vec_sub",nai_vec_sub);bindn(&vm->g,"ai.vec_mul",nai_vec_mul);bindn(&vm->g,"ai.relu",nai_relu);bindn(&vm->g,"ai.tanh",nai_tanh);bindn(&vm->g,"ai.mse",nai_mse);bindn(&vm->g,"ai.normalize",nai_normalize);bindn(&vm->g,"ai.matmul",nai_matmul);bindn(&vm->g,"ai.train_linear",nai_train_linear);bindn(&vm->g,"ai.rand",ai_rand);bindn(&vm->g,"ai.game_brain",ai_game_brain);bindn(&vm->g,"ai.game_act",ai_game_act);bindn(&vm->g,"ai.game_remember",ai_game_remember);bindn(&vm->g,"ai.game_train_step",ai_game_train_step);bindn(&vm->g,"ai.game_target_update",ai_game_target_update);bindn(&vm->g,"ai.game_config",ai_game_config);bindn(&vm->g,"ai.game_info",ai_game_info);bindn(&vm->g,"game.nav_astar",game_nav_astar);bindn(&vm->g,"games.nav_astar",game_nav_astar);bindn(&vm->g,"ai.one_hot",ai_one_hot);bindn(&vm->g,"ai.cross_entropy",ai_cross_entropy);bindn(&vm->g,"ai.dropout",ai_dropout);bindn(&vm->g,"ai.argmin",ai_argmin);bindn(&vm->g,"ai.mean",ai_mean);bindn(&vm->g,"ai.variance",ai_variance);bindn(&vm->g,"ai.clamp",ai_clamp_grad);bindn(&vm->g,"ai.model_linear",ai_model_linear);bindn(&vm->g,"ai.predict",ai_model_predict);bindn(&vm->g,"ai.train",ai_model_train_linear);bindn(&vm->g,"ai.l2",ai_l2);bindn(&vm->g,"ai.leaky_relu",ai_leaky_relu);bindn(&vm->g,"ai.accuracy",ai_accuracy);bindn(&vm->g,"ai.knn",ai_knn);bindn(&vm->g,"ai.kmeans",ai_kmeans);bindn(&vm->g,"games.vec2",ngvec2);bindn(&vm->g,"games.move",ngmove);bindn(&vm->g,"games.arrive",ngarrive);bindn(&vm->g,"games.lerp",nglerp);bindn(&vm->g,"games.circle_hit",ngcircle_hit);bindn(&vm->g,"games.aabb_hit",ngaabb_hit);bindn(&vm->g,"games.damage",ngdamage);bindn(&vm->g,"games.grid_path",nggrid_path);bindn(&vm->g,"engine.world",engine_world);bindn(&vm->g,"engine.entity",engine_entity);bindn(&vm->g,"engine.set_position",engine_set_position);bindn(&vm->g,"engine.position",engine_position);bindn(&vm->g,"engine.set_velocity",engine_set_velocity);bindn(&vm->g,"engine.velocity",engine_velocity);bindn(&vm->g,"engine.set_health",engine_set_health);bindn(&vm->g,"engine.health",engine_health);bindn(&vm->g,"engine.step",engine_step);bindn(&vm->g,"engine.info",engine_world_info);bindn(&vm->g,"engine.world_info",engine_world_info);bindn(&vm->g,"engine.gravity",engine_gravity);bindn(&vm->g,"engine.destroy",engine_destroy);bindn(&vm->g,"engine.distance",engine_distance);bindn(&vm->g,"engine.circle_collision",engine_circle_collision);bindn(&vm->g,"engine.damage",engine_damage);bindn(&vm->g,"engine.set_radius",engine_set_radius);bindn(&vm->g,"engine.nearest",engine_nearest);bindn(&vm->g,"engine.raycast",engine_raycast);bindn(&vm->g,"engine.query_radius",engine_query_radius);bindn(&vm->g,"engine.stats",engine_stats);bindn(&vm->g,"engine.nearest",engine_nearest);bindn(&vm->g,"ai.prompt",ai_prompt);bindn(&vm->g,"ai.hash_embed",ai_hash_embed);bindn(&vm->g,"ai.embed",ai_embed_server);bindn(&vm->g,"ai.cosine",ai_cosine);bindn(&vm->g,"ai.tool_call",ai_tool_call);bindn(&vm->g,"ai.gguf",ai_gguf);bindn(&vm->g,"ai.llm_info",ai_llm_info);bindn(&vm->g,"ai.llm_load",ai_llm_load);bindn(&vm->g,"ai.llm_generate",ai_llm_generate);bindn(&vm->g,"ai.llm_close",ai_llm_close);bindn(&vm->g,"ai.mlp",ai_mlp);bindn(&vm->g,"ai.mlp_predict",ai_mlp_predict);bindn(&vm->g,"ai.mlp_predict_batch",ai_mlp_predict_batch);bindn(&vm->g,"ai.mlp_predict_proba",ai_mlp_predict_proba);bindn(&vm->g,"ai.mlp_predict_proba_batch",ai_mlp_predict_proba_batch);bindn(&vm->g,"ai.predict_class",ai_predict_class);bindn(&vm->g,"ai.mlp_train",ai_mlp_train);bindn(&vm->g,"ai.mlp_train_class",ai_mlp_train_class);bindn(&vm->g,"ai.mlp_info",ai_mlp_info);bindn(&vm->g,"ai.mlp_optimizer_info",ai_mlp_optimizer_info);bindn(&vm->g,"ai.deep_mlp",ai_deep_mlp);bindn(&vm->g,"ai.deep_mlp_predict",ai_deep_mlp_predict);bindn(&vm->g,"ai.deep_mlp_predict_batch",ai_deep_mlp_predict_batch);bindn(&vm->g,"ai.deep_mlp_train",ai_deep_mlp_train);bindn(&vm->g,"ai.deep_mlp_info",ai_deep_mlp_info);bindn(&vm->g,"ai.deep_predict_class",ai_predict_class_deep);bindn(&vm->g,"ai.dataset_csv",ai_dataset_csv);bindn(&vm->g,"ai.dataset_split",ai_dataset_split);bindn(&vm->g,"ai.mlp_train_report",ai_mlp_train_report);bindn(&vm->g,"ai.mlp_save",ai_mlp_save);bindn(&vm->g,"ai.mlp_load",ai_mlp_load);bindn(&vm->g,"ai.entropy",ai_entropy);bindn(&vm->g,"ai.epsilon_greedy",ai_epsilon_greedy);bindn(&vm->g,"ai.q_target",ai_q_target);bindn(&vm->g,"ai.reward_progress",ai_reward_progress);bindn(&vm->g,"games.wander",ngwander);bindn(&vm->g,"games.separation",ngseparation);bindn(&vm->g,"engine.scene",engine_scene);bindn(&vm->g,"engine.node",engine_node);bindn(&vm->g,"engine.add_child",engine_add_child);bindn(&vm->g,"engine.node_info",engine_node_info);bindn(&vm->g,"engine.node_set_position",engine_node_set_position);bindn(&vm->g,"engine.node_position",engine_node_position);bindn(&vm->g,"engine.node_set_scale",engine_node_set_scale);bindn(&vm->g,"engine.node_set_rotation",engine_node_set_rotation);bindn(&vm->g,"engine.node_visible",engine_node_visible);bindn(&vm->g,"engine.scene_info",engine_scene_info);bindn(&vm->g,"nuclear.limits",nnuc_limits);bindn(&vm->g,"nuclear.profile",nnuc_profile);bindn(&vm->g,"nuclear.reset",nnuc_reset);bindn(&vm->g,"nuclear.capabilities",nnuc_capabilities);bindn(&vm->g,"nuclear.set_profile",nnuc_set_profile);bindn(&vm->g,"nuclear.resource",nnuc_resource);bindn(&vm->g,"nuclear.set_cpu",nnuc_set_cpu);bindn(&vm->g,"nuclear.set_memory",nnuc_set_memory);bindn(&vm->g,"nuclear.auto_memory",nnuc_auto_memory);bindn(&vm->g,"nuclear.gc",nuclear_gc);bindn(&vm->g,"jit.status",njit_status);bindn(&vm->g,"type",ntype);bindn(&vm->g,"generic.info",ngeneric_info);bindn(&vm->g,"is_type",nis_type);bindn(&vm->g,"assert_type",nassert_type);bindn(&vm->g,"oop.new",noop_new);bindn(&vm->g,"oop.get",noop_get);bindn(&vm->g,"oop.set",noop_set);bindn(&vm->g,"oop.has",noop_has);bindn(&vm->g,"oop.type",noop_type);bindn(&vm->g,"oop.class",noop_class);bindn(&vm->g,"memory.open",memory_open);bindn(&vm->g,"memory.put",memory_put);bindn(&vm->g,"memory.get",memory_get);bindn(&vm->g,"memory.search",memory_search);bindn(&vm->g,"memory.close",memory_close);bindn(&vm->g,"memory.alloc",memory_alloc);bindn(&vm->g,"memory.resize",memory_resize);bindn(&vm->g,"memory.size",memory_size);bindn(&vm->g,"memory.read_u8",memory_read_u8);bindn(&vm->g,"memory.write_u8",memory_write_u8);bindn(&vm->g,"memory.read_u16",memory_read_u16);bindn(&vm->g,"memory.write_u16",memory_write_u16);bindn(&vm->g,"memory.read_u32",memory_read_u32);bindn(&vm->g,"memory.write_u32",memory_write_u32);bindn(&vm->g,"memory.read_u64",memory_read_u64);bindn(&vm->g,"memory.write_u64",memory_write_u64);bindn(&vm->g,"memory.read_i32",memory_read_i32);bindn(&vm->g,"memory.write_i32",memory_write_i32);bindn(&vm->g,"memory.read_i64",memory_read_i64);bindn(&vm->g,"memory.write_i64",memory_write_i64);bindn(&vm->g,"memory.read_f32",memory_read_f32);bindn(&vm->g,"memory.write_f32",memory_write_f32);bindn(&vm->g,"memory.read_f64",memory_read_f64);bindn(&vm->g,"memory.write_f64",memory_write_f64);bindn(&vm->g,"memory.fill",memory_fill);bindn(&vm->g,"memory.copy",memory_copy);bindn(&vm->g,"memory.to_array",memory_to_array);bindn(&vm->g,"memory.free",memory_free);bindn(&vm->g,"memory.wrap",memory_wrap);bindn(&vm->g,"memory.address",memory_address);bindn(&vm->g,"ai.game_brain",ai_game_brain);bindn(&vm->g,"ai.game_act",ai_game_act);bindn(&vm->g,"ai.game_remember",ai_game_remember);bindn(&vm->g,"ai.game_train_step",ai_game_train_step);bindn(&vm->g,"ai.game_target_update",ai_game_target_update);bindn(&vm->g,"ai.game_info",ai_game_info);bindn(&vm->g,"ai.game_config",ai_game_config);bindn(&vm->g,"ai.agent",agent_create);bindn(&vm->g,"ai.agent_step",agent_step);bindn(&vm->g,"ai.agent_info",agent_info);bindn(&vm->g,"ai.agent_close",agent_close);bindn(&vm->g,"async.spawn",nasync_spawn);bindn(&vm->g,"async.join",nasync_join);bindn(&vm->g,"async.await",nasync_await);bindn(&vm->g,"async.done",nasync_done);bindn(&vm->g,"assert",nassert);bindn(&vm->g,"error",nerror);bindn(&vm->g,"throw",nthrow);bindn(&vm->g,"range",nrange);bindn(&vm->g,"tuple",ntuple);bindn(&vm->g,"gpu.backends",ngpu_backends);bindn(&vm->g,"board.info",board_info);bindn(&vm->g,"gpu.available",ngpu_available);bindn(&vm->g,"gpu.info",ngpu_info);bindn(&vm->g,"gpu.backend",ngpu_backend);bindn(&vm->g,"gpu.compute",ngpu_compute);bindn(&vm->g,"cpu.info",ncpu_info);bindn(&vm->g,"cpu.cores",ncpu_cores);bindn(&vm->g,"cpu.arch",ncpu_arch);bindn(&vm->g,"cpu.affinity",ncpu_affinity);bindn(&vm->g,"cpu.yield",ncpu_yield);bindn(&vm->g,"simd.add",nsimd_add);bindn(&vm->g,"games.vec3",games_vec3);bindn(&vm->g,"games.move3",games_move3);bindn(&vm->g,"games.distance3",games_distance3);bindn(&vm->g,"games.lerp_vec",games_lerp_vec);bindn(&vm->g,"games.stats",games_stats);bindn(&vm->g,"game.spawn",game_spawn);bindn(&vm->g,"game.entities",game_entities);bindn(&vm->g,"game.snapshot",game_snapshot);bindn(&vm->g,"game.fixed_step",game_fixed_step);bindn(&vm->g,"game.collisions",game_collisions);bindn(&vm->g,"game.spawn_batch",game_spawn_batch);bindn(&vm->g,"game.distance",game_distance);bindn(&vm->g,"game.nearest",game_nearest);bindn(&vm->g,"game.raycast",game_raycast);bindn(&vm->g,"game.set_gravity",game_set_gravity);bindn(&vm->g,"ai.prompt_template",ai_prompt_template);bindn(&vm->g,"ai.rag_topk",ai_rag_topk);bindn(&vm->g,"ai.token_count",ai_token_count);bindn(&vm->g,"ai.gelu",nai_gelu);bindn(&vm->g,"ai.layernorm",nai_layernorm);bindn(&vm->g,"ai.softmax_temperature",nai_softmax_temperature);bindn(&vm->g,"ai.topk",nai_topk);bindn(&vm->g,"ai.sample_greedy",nai_sample_greedy);bindn(&vm->g,"ai.attention",nai_attention);bindn(&vm->g,"ai.conv2d",ai_conv2d);bindn(&vm->g,"ai.rnn",ai_rnn);bindn(&vm->g,"ai.rnn_forward",ai_rnn_forward);bindn(&vm->g,"ai.lstm",ai_lstm);bindn(&vm->g,"ai.lstm_forward",ai_lstm_forward);bindn(&vm->g,"ai.transformer",ai_transformer);bindn(&vm->g,"ai.transformer_forward",ai_transformer_forward);bindn(&vm->g,"ai.model_zoo",ai_model_zoo);bindn(&vm->g,"ai.hf_info",ai_hf_info);bindn(&vm->g,"ai.hf_download",ai_hf_download);bindn(&vm->g,"onnx.import",ai_onnx_import);bindn(&vm->g,"onnx.export_mlp",ai_onnx_export_mlp);bindn(&vm->g,"onnx.run",ai_onnx_run);bindn(&vm->g,"gpu.cuda_info",ngpu_cuda_info);bindn(&vm->g,"gpu.cuda_add",ngpu_cuda_add);bindn(&vm->g,"distributed.init",distributed_init);bindn(&vm->g,"distributed.allreduce",distributed_allreduce);bindn(&vm->g,"distributed.barrier",distributed_barrier);bindn(&vm->g,"distributed.info",distributed_info);bindn(&vm->g,"distributed.close",distributed_close);bindn(&vm->g,"tensor.zeros",ntensor_zeros);bindn(&vm->g,"tensor.range",ntensor_range);bindn(&vm->g,"tensor.mean",ntensor_mean);bindn(&vm->g,"tensor.from_array",tensor_from_array);bindn(&vm->g,"tensor.ones",tensor_ones);bindn(&vm->g,"tensor.parameter",tensor_parameter);bindn(&vm->g,"tensor.add",tensor_add);bindn(&vm->g,"tensor.sub",tensor_sub);bindn(&vm->g,"tensor.mul",tensor_mul);bindn(&vm->g,"tensor.neg",tensor_neg);bindn(&vm->g,"tensor.relu",tensor_relu);bindn(&vm->g,"tensor.tanh",tensor_tanh);bindn(&vm->g,"tensor.sigmoid",tensor_sigmoid);bindn(&vm->g,"tensor.gelu",tensor_gelu);bindn(&vm->g,"tensor.sum",tensor_sum);bindn(&vm->g,"tensor.mean_real",tensor_mean_real);bindn(&vm->g,"tensor.cross_entropy",tensor_cross_entropy);bindn(&vm->g,"tensor.matmul",tensor_matmul);bindn(&vm->g,"tensor.linear",tensor_linear);bindn(&vm->g,"tensor.softmax",tensor_softmax);bindn(&vm->g,"tensor.layernorm_real",tensor_layernorm);bindn(&vm->g,"tensor.backward",tensor_backward);bindn(&vm->g,"tensor.zero_grad",tensor_zero_grad);bindn(&vm->g,"tensor.data",tensor_data);bindn(&vm->g,"tensor.grad",tensor_grad);bindn(&vm->g,"tensor.shape",tensor_shape);bindn(&vm->g,"tensor.numel",tensor_numel);bindn(&vm->g,"tensor.adam_step",tensor_adam_step);bindn(&vm->g,"tensor.info",tensor_info);bindn(&vm->g,"tensor.clear",tensor_clear);bindn(&vm->g,"tensor.to_fp16",tensor_to_fp16);bindn(&vm->g,"tensor.to_bf16",tensor_to_bf16);bindn(&vm->g,"tensor.from_mixed",tensor_from_mixed);bindn(&vm->g,"tensor.mixed_matmul",tensor_mixed_matmul);bindn(&vm->g,"tensor.mixed_info",tensor_mixed_info);bindn(&vm->g,"tensor.linear_relu",tensor_linear_relu);bindn(&vm->g,"tensor.optimize",tensor_optimize_graph);bindn(&vm->g,"security.hash_text",sec_hash_text);bindn(&vm->g,"security.file_hash",sec_file_hash);bindn(&vm->g,"security.file_entropy",sec_file_entropy);bindn(&vm->g,"security.scan_source",sec_scan_source);bindn(&vm->g,"security.log_analyze",sec_log_analyze);bindn(&vm->g,"security.private_target",sec_private_target);bindn(&vm->g,"security.lab_scenario",sec_lab_scenario);bindn(&vm->g,"security.ioc_extract",sec_ioc_extract);bindn(&vm->g,"security.xss_scan",security_xss_scan);bindn(&vm->g,"security.html_escape",security_html_escape);bindn(&vm->g,"defense.hash_text",sec_hash_text);bindn(&vm->g,"defense.file_hash",sec_file_hash);bindn(&vm->g,"defense.scan_source",sec_scan_source);bindn(&vm->g,"defense.log_analyze",sec_log_analyze);bindn(&vm->g,"audit.event",audit_event);bindn(&vm->g,"compliance.status",compliance_status);bindn(&vm->g,"report.json",report_json);bindn(&vm->g,"regex.compile",regex_compile);bindn(&vm->g,"regex.find_all",regex_find_all);bindn(&vm->g,"regex.match",regex_match);bindn(&vm->g,"regex.search",regex_search);bindn(&vm->g,"datetime.now",datetime_now);bindn(&vm->g,"datetime.format",datetime_format);bindn(&vm->g,"datetime.add_days",datetime_add_days);bindn(&vm->g,"datetime.add_seconds",datetime_add_seconds);bindn(&vm->g,"datetime.diff",datetime_diff);bindn(&vm->g,"gfx.window",gfx_window);bindn(&vm->g,"gfx.draw_circle",gfx_draw_circle);bindn(&vm->g,"gfx.draw_text",gfx_draw_text);bindn(&vm->g,"security.regex_ioc",sec_regex_ioc);bindn(&vm->g,"security.secret_scan",sec_secret_scan);bindn(&vm->g,"defense.regex_ioc",sec_regex_ioc);bindn(&vm->g,"defense.secret_scan",sec_secret_scan);bindn(&vm->g,"security.port_risk_score",security_port_risk_score);bindn(&vm->g,"defense.port_risk_score",security_port_risk_score);bindn(&vm->g,"security.password_strength",security_password_strength);bindn(&vm->g,"defense.password_strength",security_password_strength);bindn(&vm->g,"security.crack_hash",security_crack_hash);bind_v91_multiplayer(vm);bind_v10_server(vm);build_builtin_namespaces(vm);}
static long long cpu_ms(void){return (long long)((double)clock()*1000.0/(double)CLOCKS_PER_SEC);}
static long long wall_ms(void){
#ifdef CLOCK_MONOTONIC
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
    return (long long)ts.tv_sec*1000LL+(long long)ts.tv_nsec/1000000LL;
#else
    return cpu_ms();
#endif
}
static int apply_cpu_affinity(VM*vm){
    if(!vm)return 0;if(vm->cpu_cores<=0){vm->cpu_affinity_ok=1;return 1;}
#ifdef _WIN32
    SYSTEM_INFO si;GetSystemInfo(&si);int n=(int)si.dwNumberOfProcessors;if(n<1)n=1;if(vm->cpu_cores>n)vm->cpu_cores=n;DWORD_PTR mask=0;for(int i=0;i<vm->cpu_cores&&i<(int)(sizeof(DWORD_PTR)*8);i++)mask|=((DWORD_PTR)1<<i);vm->cpu_affinity_ok=(mask!=0&&SetProcessAffinityMask(GetCurrentProcess(),mask)!=0);return vm->cpu_affinity_ok;
#elif defined(__linux__) && !defined(__ANDROID__)
    int n=(int)sysconf(_SC_NPROCESSORS_ONLN);if(n<1)n=1;if(vm->cpu_cores>n)vm->cpu_cores=n;cpu_set_t set;CPU_ZERO(&set);for(int i=0;i<vm->cpu_cores;i++)CPU_SET(i,&set);vm->cpu_affinity_ok=(sched_setaffinity(0,sizeof(set),&set)==0);return vm->cpu_affinity_ok;
#else
    vm->cpu_affinity_ok=0;return 0;
#endif
}
static void runtime_budget_check(VM*vm){
    if(vm->cpu_limit_ms>0 && cpu_ms()-vm->start_cpu_ms>vm->cpu_limit_ms)
        die("Haris: CPU time limit exceeded (%lld ms)",vm->cpu_limit_ms);
    if(vm->cpu_percent>0 && vm->cpu_percent<100){
        long long noww=wall_ms(), nowc=cpu_ms();
        if(vm->cpu_slice_wall_ms==0){vm->cpu_slice_wall_ms=noww;vm->cpu_slice_cpu_ms=nowc;}
        long long w=noww-vm->cpu_slice_wall_ms, c=nowc-vm->cpu_slice_cpu_ms;
        if(w>=50){
            long long allowed=(w*vm->cpu_percent)/100;
            if(c>allowed){
                long long sleep_ms=c-allowed;
                if(sleep_ms>0){struct timespec ts={(time_t)(sleep_ms/1000),(long)((sleep_ms%1000)*1000000L)};nanosleep(&ts,NULL);}
            }
            vm->cpu_slice_wall_ms=wall_ms(); vm->cpu_slice_cpu_ms=cpu_ms();
        }
    }
}

static void gc_mark_late_handle(void*p,int kind){
#ifdef HK_GFX
    if(kind==HK_GFX){HGfxWindow*w=p;if(w->title)gc_mark_ptr(w->title);return;}
#endif
    if(kind==HK_MEMBUF){HMemBuf*m=p;if(m->owned&&m->data)gc_mark_ptr(m->data);return;}
    if(kind==HK_ASYNC){AsyncTask*t=p;if(t->file)gc_mark_ptr(t->file);if(t->fn)gc_mark_fn(t->fn);if(t->args){gc_mark_ptr(t->args);for(int i=0;i<t->argc;i++)gc_mark_value(t->args[i]);}gc_mark_value(t->result);return;}
}

static void runtime_safe_point(VM*vm,int sp,int fp){if(!vm)return;if(g_mem_used>=g_gc_next){vm->sp=sp;vm->fp=fp;if(gc_minor(vm)==0&&g_mem_used>=g_gc_next*2)gc_collect(vm);}}

static HARIS_HOT Value vm_execute(VM*vm,Fn*f,int argc,Value*args){
    EXEC_LOCK();
    vm->int_overflow=0;
    vm->last_error=0;
    VM *prev_runtime=g_runtime_vm; size_t prev_mem_limit=g_mem_limit; int saved_err_depth=vm->err_depth;
    if(vm->jmp_depth>=32)die("Haris runtime: error-handler nesting overflow");
    int jmp_slot=vm->jmp_depth++;
    vm->err_depth=0; vm->err_active=0;
    g_runtime_vm=vm; g_mem_limit=vm->mem_limit_bytes; active_vm_add(vm);
    vm->fp=1; vm->sp=0;
    vm->start_cpu_ms=cpu_ms();
    g_mem_limit=vm->mem_limit_bytes;
#define HPUSH(v) do{if(HARIS_UNLIKELY(sp>=H_STACK_MAX))die("Haris runtime: value stack overflow");st[sp++]=(v);}while(0)
#define HPOP() (HARIS_UNLIKELY(sp<=0)?(die("Haris runtime: value stack underflow"),vn()):st[--sp])
#define HRETURN(v) do{vm->sp=sp;vm->fp=fp;vm->err_depth=saved_err_depth;vm->err_active=0;if(vm->jmp_depth>0)vm->jmp_depth--;active_vm_del(vm);g_runtime_vm=prev_runtime;g_mem_limit=prev_mem_limit;EXEC_UNLOCK();return(v);}while(0)
    /* Hot-loop optimization: keep stack pointer, stack base and frame array in
       locals.  The old interpreter performed a VM struct load/store for nearly
       every bytecode instruction. */
    Value *st=vm->st;
    int sp=0, fp=1;
    Frame *frames=vm->fr;
    frames[0]=(Frame){0,0,f};
    if(argc<0||argc>H_CALL_MAX)die("Haris runtime: invalid argument count");
    if(!f->is_root) HPUSH(vn());
    for(int i=0;i<argc;i++) HPUSH(args[i]);
    while(sp < (f->is_root ? f->nlocals : 1+f->np+f->nlocals)) HPUSH(vn());

#if (defined(__GNUC__) || defined(__clang__)) && !defined(HARIS_PORTABLE)
    /* Direct-threaded dispatch avoids the branch-heavy opcode switch on GCC/Clang. */
    static void *dispatch[] = {
        &&L_CONST,&&L_GET,&&L_GETL,&&L_SETG,&&L_SETL,&&L_MAKE_CLOSURE,&&L_POP,&&L_ADD,&&L_SUB,&&L_MUL,&&L_DIV,
        &&L_MOD,&&L_NEG,&&L_NOT,&&L_EQ,&&L_NEQ,&&L_LT,&&L_LTE,&&L_GT,&&L_GTE,&&L_AND,&&L_OR,
        &&L_JMP,&&L_JMPF,&&L_CALL,&&L_RET,&&L_ARRAY,&&L_INDEX,&&L_SETINDEX,&&L_FIELD,&&L_SETFIELD,&&L_STRUCT,&&L_TYPECHECK,&&L_TRY,&&L_ENDTRY,&&L_JMPNULL,&&L_HALT,
        &&L_INC_LOCAL,&&L_DEC_LOCAL,&&L_LOCAL_CONST_ADD,&&L_LOCAL_CONST_SUB,&&L_LOCAL_CONST_MUL,&&L_LOCAL_CONST_DIV,&&L_LOCAL_CONST_MOD,&&L_LOCAL_BIN,&&L_LOCAL_CMP_JMPF
    };
    _Static_assert(sizeof(dispatch)/sizeof(dispatch[0]) == (size_t)I_OP_COUNT,
                   "Haris: direct dispatch table out of sync with Op enum");
#define DISPATCH() do{if((++ticks & 4095u)==0){runtime_budget_check(vm);if(g_mem_used>=g_gc_next){vm->sp=sp;vm->fp=fp;if(gc_minor(vm)==0 || g_mem_used>=g_gc_next*2)gc_collect(vm);}}goto *dispatch[code[ip].op];}while(0)
    Fn *fn=f; Ins *code=fn->ch.v; Value *cons=fn->ch.c; int ip=0, base=0; unsigned long long ticks=0; g_runtime_vm=vm;
    if(setjmp(vm->jmp_stack[jmp_slot])!=0){ if(!vm->err_active){vm->last_error=1;HRETURN(vn());} vm->err_active=0; if(vm->err_depth<=0){vm->last_error=1;fprintf(stderr,"Haris runtime error: %s\n",vm->err_msg);HRETURN(vn());}
        struct ErrFrame eh=vm->err_frames[vm->err_depth-1];vm->err_depth--;vm->err_ip=eh.ip;vm->err_fp=eh.fp;vm->err_sp=eh.sp;vm->sp=eh.sp;sp=eh.sp;fp=eh.fp;Frame *ef=&frames[fp-1];fn=ef->fn;code=fn->ch.v;cons=fn->ch.c;ip=eh.ip;base=ef->base; HPUSH(vs(vm->err_msg)); }
    DISPATCH();
L_CONST: HPUSH(cons[code[ip].a]); ip++; DISPATCH();
L_GETL: {int idx=base+1+code[ip].a;if(idx>=sp)die("Haris runtime: invalid argument index");HPUSH(st[idx]);ip++;DISPATCH();}
L_SETL: {int idx=base+1+code[ip].a;if(idx<base+1||idx>=sp)die("Haris runtime: invalid local slot");st[idx]=st[--sp];ip++;DISPATCH();}
L_INC_LOCAL: {int idx=base+1+code[ip].a;if(idx<base+1||idx>=sp)die("Haris runtime: invalid local slot");Value v=st[idx];if(v.t==VINT)st[idx]=vi(int_binop(vm,I_ADD,v.u.i,1));else if(v.t==VFLOAT)st[idx]=vf(v.u.f+1.0);else die("numeric operator needs number");ip++;DISPATCH();}
L_DEC_LOCAL: {int idx=base+1+code[ip].a;if(idx<base+1||idx>=sp)die("Haris runtime: invalid local slot");Value v=st[idx];if(v.t==VINT)st[idx]=vi(int_binop(vm,I_SUB,v.u.i,1));else if(v.t==VFLOAT)st[idx]=vf(v.u.f-1.0);else die("numeric operator needs number");ip++;DISPATCH();}
#define LOCAL_CONST_BIN(OPCODE,OP) do{int idx=base+1+code[ip].a,ci=code[ip].b;if(idx<base+1||idx>=sp||ci<0||ci>=fn->ch.nc)die("Haris runtime: invalid specialized arithmetic operands");Value v=st[idx],cst=cons[ci];if(v.t==VINT&&cst.t==VINT)st[idx]=vi(int_binop(vm,(OP),v.u.i,cst.u.i));else if(isnum(v)&&cst.t==VINT){double y=(double)cst.u.i;if((OP)==I_DIV&&y==0.0)die("division by zero");st[idx]=vf((OP)==I_ADD?v.u.f+y:(OP)==I_SUB?v.u.f-y:(OP)==I_MUL?v.u.f*y:(OP)==I_DIV?v.u.f/y:fmod(v.u.f,y));}else die("numeric operator needs numbers");ip++;DISPATCH();}while(0)
L_LOCAL_CONST_ADD: LOCAL_CONST_BIN(I_LOCAL_CONST_ADD,I_ADD);
L_LOCAL_CONST_SUB: LOCAL_CONST_BIN(I_LOCAL_CONST_SUB,I_SUB);
L_LOCAL_CONST_MUL: LOCAL_CONST_BIN(I_LOCAL_CONST_MUL,I_MUL);
L_LOCAL_CONST_DIV: LOCAL_CONST_BIN(I_LOCAL_CONST_DIV,I_DIV);
L_LOCAL_CONST_MOD: LOCAL_CONST_BIN(I_LOCAL_CONST_MOD,I_MOD);
#undef LOCAL_CONST_BIN
L_LOCAL_BIN: {
    int di=base+1+code[ip].a,si=base+1+code[ip].b;
    if(di<base+1||di>=sp||si<base+1||si>=sp)die("Haris runtime: invalid local binary operands");
    Value a=st[di],b=st[si];Op op=(Op)(uintptr_t)code[ip].cache;
    if(op<I_ADD||op>I_MOD)die("Haris runtime: invalid local binary opcode");
    if(a.t==VINT&&b.t==VINT)st[di]=vi(int_binop(vm,op,a.u.i,b.u.i));
    else{if(!isnum(a)||!isnum(b))die("numeric operator needs numbers");double x=dn(a),y=dn(b);if(op==I_DIV&&y==0.0)die("division by zero");st[di]=vf(op==I_ADD?x+y:op==I_SUB?x-y:op==I_MUL?x*y:op==I_DIV?x/y:fmod(x,y));}
    ip++;DISPATCH();
}
L_LOCAL_CMP_JMPF: {
    int idx=base+1+code[ip].a,ci=code[ip].b;
    if(idx<base+1||idx>=sp||ci<0||ci>=fn->ch.nc)die("Haris runtime: invalid local comparison operands");
    Value a=st[idx],b=cons[ci];unsigned ck=code[ip].cache_kind;int r=0;
    if(ck<1u||ck>6u)die("Haris runtime: invalid local comparison opcode");
    Op op=(Op)(I_EQ+(int)ck-1);
    if(a.t==VINT&&b.t==VINT){r=op==I_EQ?a.u.i==b.u.i:op==I_NEQ?a.u.i!=b.u.i:op==I_LT?a.u.i<b.u.i:op==I_LTE?a.u.i<=b.u.i:op==I_GT?a.u.i>b.u.i:a.u.i>=b.u.i;}
    else{if(!isnum(a)||!isnum(b))die("comparison needs numbers");double x=dn(a),y=dn(b);r=op==I_EQ?x==y:op==I_NEQ?x!=y:op==I_LT?x<y:op==I_LTE?x<=y:op==I_GT?x>y:x>=y;}
    if(!r)ip=(int)(uintptr_t)code[ip].cache;else ip++;DISPATCH();
}
L_GET: {const char*key=fn->ch.names[code[ip].a];if(fn->closure){Value ev;if(env_lookup_chain(fn->closure,key,&ev)){HPUSH(ev);ip++;DISPATCH();}}int ci;if(code[ip].cache_kind==2u)ci=(int)(uintptr_t)code[ip].cache-1;else{ci=code[ip].line-1;if(ci<0||ci>=vm->g.n||strcmp(vm->g.v[ci].k,key)){ci=egi(&vm->g,key);if(ci<0)die("Haris runtime: undefined '%s'",key);code[ip].line=ci+1;}code[ip].cache=(void*)(uintptr_t)(ci+1);code[ip].cache_kind=2u;}if((unsigned)ci>=(unsigned)vm->g.n||strcmp(vm->g.v[ci].k,key))die("Haris runtime: invalid global cache");HPUSH(vm->g.v[ci].v);ip++;DISPATCH();}
L_SETG: {Value v=st[--sp];const char*key=fn->ch.names[code[ip].a];int ci;if(code[ip].cache_kind==2u)ci=(int)(uintptr_t)code[ip].cache-1;else{ci=code[ip].line-1;if(ci<0||ci>=vm->g.n||strcmp(vm->g.v[ci].k,key))ci=egi(&vm->g,key);if(ci<0){en(&vm->g,key,v);ci=egi(&vm->g,key);}else vm->g.v[ci].v=v;code[ip].line=ci+1;code[ip].cache=(void*)(uintptr_t)(ci+1);code[ip].cache_kind=2u;ip++;DISPATCH();}if((unsigned)ci>=(unsigned)vm->g.n||strcmp(vm->g.v[ci].k,key))die("Haris runtime: invalid global cache");vm->g.v[ci].v=v;ip++;DISPATCH();}
L_POP: if(sp<=0)die("Haris runtime: value stack underflow"); --sp; ip++; DISPATCH();
L_NEG: {Value a=HPOP();if(a.t==VINT){if(a.u.i==LLONG_MIN){if(vm->module_sandboxed){vm->int_overflow=1;HPUSH(vi(0));}else die("Haris runtime: integer overflow");}else HPUSH(vi(-a.u.i));}else HPUSH(vf(-dn(a)));ip++;DISPATCH();}
L_NOT: {Value a=HPOP();HPUSH(vb(!truth(a)));ip++;DISPATCH();}
L_ADD: case_dummy_add: {
    Value b=st[--sp],a=st[--sp]; Op op=code[ip].op;
    if(op==I_ADD&&a.t==VSTR&&b.t==VSTR){size_t na=strlen(a.u.s),nb=strlen(b.u.s);char*z=xmalloc(na+nb+1);memcpy(z,a.u.s,na);memcpy(z+na,b.u.s,nb);z[na+nb]=0;HPUSH(((Value){.t=VSTR,.u.s=z}));ip++;DISPATCH();}
    if(a.t==VINT&&b.t==VINT){HPUSH(vi(int_binop(vm,op,a.u.i,b.u.i)));ip++;DISPATCH();}
    if(!isnum(a)||!isnum(b))die("numeric operator needs numbers");
    double x=dn(a),y=dn(b);
    if(op==I_DIV&&y==0)die("division by zero");
    HPUSH(vf(op==I_ADD?x+y:op==I_SUB?x-y:op==I_MUL?x*y:op==I_DIV?x/y:fmod(x,y)));
    ip++;DISPATCH();
}
L_SUB: goto case_dummy_add; L_MUL: goto case_dummy_add; L_DIV: goto case_dummy_add; L_MOD: goto case_dummy_add;
L_EQ: case_dummy_eq: {
    if(sp<2)die("Haris runtime: value stack underflow");
    Value b=st[--sp],a=st[--sp];int r=0;
    if(a.t==VINT&&b.t==VINT)r=(a.u.i==b.u.i);
    else if(isnum(a)&&isnum(b))r=(dn(a)==dn(b));
    else if(a.t==b.t){if(a.t==VNULL)r=1;else if(a.t==VBOOL)r=a.u.b==b.u.b;else if(a.t==VSTR)r=!strcmp(a.u.s,b.u.s);}
    if(code[ip].op==I_NEQ)r=!r;HPUSH(vb(r));ip++;DISPATCH();
}
L_NEQ: goto case_dummy_eq;
L_LT: case_dummy_cmp: {if(sp<2)die("Haris runtime: value stack underflow");Value b=st[--sp],a=st[--sp];if(a.t==VINT&&b.t==VINT){Op op=code[ip].op;int r=op==I_LT?a.u.i<b.u.i:op==I_LTE?a.u.i<=b.u.i:op==I_GT?a.u.i>b.u.i:a.u.i>=b.u.i;HPUSH(vb(r));ip++;DISPATCH();}if(!isnum(a)||!isnum(b))die("comparison needs numbers");double x=dn(a),y=dn(b);Op op=code[ip].op;int r=op==I_LT?x<y:op==I_LTE?x<=y:op==I_GT?x>y:x>=y;HPUSH(vb(r));ip++;DISPATCH();}
L_LTE: goto case_dummy_cmp; L_GT: goto case_dummy_cmp; L_GTE: goto case_dummy_cmp;
L_AND: {Value b=HPOP(),a=HPOP();HPUSH(vb(truth(a)&&truth(b)));ip++;DISPATCH();}
L_OR:  {Value b=HPOP(),a=HPOP();HPUSH(vb(truth(a)||truth(b)));ip++;DISPATCH();}
L_JMP: { int target=code[ip].a;
 if(target<ip){ int ex=ip+1;
  if(!fn->loop_acc_active && ++fn->loop_hits>=100){
   if(code[target].op==I_GETL && target+6==ip && (code[target+1].op==I_CONST||code[target+1].op==I_GETL) && code[target+2].op>=I_LT && code[target+2].op<=I_LTE && code[target+3].op==I_JMPF && code[target+4].op==I_LOCAL_BIN && code[target+5].op==I_INC_LOCAL && code[target+6].op==I_JMP){
    int vi=code[target].a,ac=code[target+4].a;int ci=code[target+1].a;long long lim=0;int lim_local=-1;int lim_is_local=0;int lim_valid=1;if(code[target+1].op==I_CONST){if(ci<0||ci>=fn->ch.nc||fn->ch.c[ci].t!=VINT)lim_valid=0;else lim=fn->ch.c[ci].u.i;}else if(code[target+1].op==I_GETL){lim_is_local=1;lim_local=code[target+1].a;if(lim_local<0||lim_local==vi||lim_local==ac)lim_valid=0;}else lim_valid=0;long long step=(code[target+5].a==vi)?1:0;
    if(code[target+2].op!=I_LT&&code[target+2].op!=I_LTE)step=0;
    if(lim_valid&&code[target+3].a==ip+1&&code[target+4].b==vi&&(Op)(uintptr_t)code[target+4].cache==I_ADD&&code[target+5].a==vi&&code[target+6].a==target)jit_compile_accum_loop(fn,target,code[target+3].a,ac,vi,1,code[target+2].op,lim,step,lim_is_local,lim_local);
   }
  }

  if(ex<fn->ch.n){
   /* Optimized local induction loop: GETL/CONST/CMP/JMPF/INC|DEC/JMP. */
   if(code[target].op==I_GETL && target+5<ex && (code[target+1].op==I_CONST || code[target+1].op==I_GETL) && (code[target+2].op==I_LT||code[target+2].op==I_LTE) && code[target+3].op==I_JMPF && code[target+3].a>ip && code[target+4].op==I_INC_LOCAL && code[target+4].a==code[target].a && code[target+5].op==I_JMP && code[target+5].a==target){
      long long lim=0;int lim_local=-1;int lim_is_local=(code[target+1].op==I_GETL);int lim_valid=1;
      if(lim_is_local){lim_local=code[target+1].a;if(lim_local<0||lim_local==code[target].a)lim_valid=0;}
      else {int ci=code[target+1].a;if(ci<0||ci>=fn->ch.nc||fn->ch.c[ci].t!=VINT)lim_valid=0;else lim=fn->ch.c[ci].u.i;}
      if(++fn->loop_hits>=100&&!fn->loop_jit_i64&&!fn->loop_jit3_i64&&lim_valid)jit_compile_local_loop_i64(fn,target,code[target+3].a,code[target].a,lim,1,code[target+2].op,lim_is_local,lim_local);
   } else if(code[target].op==I_LOCAL_CMP_JMPF && target+2==ip && code[target+1].op==I_INC_LOCAL && code[target+1].a==code[target].a && code[target+2].op==I_JMP && code[target+2].a==target){
      int cmp=(int)code[target].cache_kind>0 ? I_EQ+(int)code[target].cache_kind-1 : I_HALT;int ci=code[target].b;long long lim=0;int lim_valid=(cmp==I_LT||cmp==I_LTE);
      if(ci<0||ci>=fn->ch.nc||fn->ch.c[ci].t!=VINT)lim_valid=0;else lim=fn->ch.c[ci].u.i;
      int loop_exit=(int)(uintptr_t)code[target].cache;
      if(++fn->loop_hits>=100&&!fn->loop_jit_i64&&!fn->loop_jit3_i64&&lim_valid&&loop_exit==ex)jit_compile_fused_local_loop_i64(fn,target,loop_exit,code[target].a,lim,cmp);
   } else if(code[target].op==I_GET && target+3<ex){
      int var=code[target].a;
      if(code[target+1].op==I_CONST && code[target+1].a>=0 && code[target+1].a<fn->ch.nc && fn->ch.c[code[target+1].a].t==VINT && (code[target+2].op==I_LT||code[target+2].op==I_LTE) && code[target+3].op==I_JMPF && code[target+3].a>ip){
       long long lim=fn->ch.c[code[target+1].a].u.i; int cmp=code[target+2].op; int setpos=ip-1;
       if(setpos>=target && code[setpos].op==I_SET && code[setpos].a==var && setpos-1>=target && code[setpos-1].op==I_ADD){
        int cidx=code[setpos-2].a; if(cidx>=0&&cidx<fn->ch.nc&&fn->ch.c[cidx].t==VINT){long long step=fn->ch.c[cidx].u.i; jit_try_loop(fn,target,code[target+3].a,var,lim,step,cmp);}
       }
      }
   }
  }
  if(fn->loop_acc_active&&target==fn->loop_acc_start){
   int ai=base+1+fn->loop_acc_name,vi_idx=base+1+fn->loop_acc_var;
   Value av=(ai>=base+1&&ai<sp)?st[ai]:vn(),iv=(vi_idx>=base+1&&vi_idx<sp)?st[vi_idx]:vn();
   long long alimit=fn->loop_acc_limit;if(fn->loop_acc_limit_is_local){int li=base+1+fn->loop_acc_limit_local;Value lv=(li>=base+1&&li<sp)?st[li]:vn();if(lv.t!=VINT){fn->loop_acc_active=0;goto loop_acc_done;}alimit=lv.u.i;}
   if(av.t==VINT&&iv.t==VINT){long long fa=0,fv=0;if(jit_accum_add_remaining(iv.u.i,alimit,fn->loop_acc_step,fn->loop_acc_cmp,av.u.i,&fa,&fv)){st[ai]=vi(fa);st[vi_idx]=vi(fv);ip=fn->loop_acc_exit;DISPATCH();}}
   fn->loop_acc_active=0;
loop_acc_done: ;
  }
  if((fn->loop_jit_i64||fn->loop_jit3_i64)&&target==fn->loop_start){
   if(fn->loop_is_local){
    int li=base+1+fn->loop_var_name;Value lv=(li>=base+1&&li<sp)?st[li]:vn();
    long long local_limit=fn->loop_limit;int limit_ok=1;
    if(fn->loop_limit_is_local){int lidx=base+1+fn->loop_limit_local;Value limv=(lidx>=base+1&&lidx<sp)?st[lidx]:vn();if(limv.t!=VINT)limit_ok=0;else local_limit=limv.u.i;}
    /* `i <= LLONG_MAX` followed by i++ must still raise Haris' checked
       integer-overflow error at the final increment. The 2-arg native
       fixed-point loop cannot represent that overflow, so deopt this case
       back to the ordinary VM path. */
    if(limit_ok&&fn->loop_cmp==I_LTE&&local_limit==LLONG_MAX)limit_ok=0;
    if(limit_ok&&lv.t==VINT){long long curv=lv.u.i;long long finalv=fn->loop_jit3_i64?fn->loop_jit3_i64(curv,local_limit,fn->loop_step):fn->loop_jit_i64(curv,local_limit);st[li]=vi(finalv);ip=fn->loop_exit;DISPATCH();}
   }
   else {int gi=egi(&vm->g,fn->ch.names[fn->loop_var_name]);Value gv=(gi>=0?vm->g.v[gi].v:vn());if(gv.t==VINT){long long curv=gv.u.i;long long finalv=fn->loop_jit3_i64?fn->loop_jit3_i64(curv,fn->loop_limit,fn->loop_step):fn->loop_jit_i64(curv,fn->loop_limit);if(gi>=0)vm->g.v[gi].v=vi(finalv);else en(&vm->g,fn->ch.names[fn->loop_var_name],vi(finalv));ip=fn->loop_exit;DISPATCH();}}
  }
 }
 ip=target; DISPATCH(); }
L_JMPF: {Value a=HPOP();if(!truth(a))ip=code[ip].a;else ip++;DISPATCH();}
L_MAKE_CLOSURE: {if(sp<1||st[sp-1].t!=VFN)die("Haris runtime: closure expects function");Fn*proto=st[sp-1].u.fn;Fn*c=xmalloc(sizeof(*c));memcpy(c,proto,sizeof(*c));c->name=proto->name;c->params=proto->params;c->locals=proto->locals;c->gens=proto->gens;c->ch=proto->ch;c->closure=xmalloc(sizeof(Env));memset(c->closure,0,sizeof(Env));c->closure->p=fn->closure;if(fn->np+fn->nlocals>0){for(int i=0;i<fn->np+fn->nlocals;i++){const char*k=i<fn->np?fn->params[i]:fn->locals[i-fn->np];int idx=base+1+i;if(idx<sp)en(c->closure,k,st[idx]);}}st[sp-1]=(Value){.t=VFN,.u.fn=c};ip++;DISPATCH();}
L_CALL: {
    int n=code[ip].a;if(n<0||n>H_CALL_MAX||sp<n+1)die("Haris runtime: stack underflow/invalid argc in call");
    Ins *call_ins=&code[ip]; Value *args2=&st[sp-n]; Value cal=st[sp-n-1];
    if(cal.t==VFN && cal.u.fn->ngens>0){
        const char *specs[16]={0}; int ns=0;
        if(code[ip].b>0){
            int cidx=code[ip].b-1;if(cidx<0||cidx>=fn->ch.nc||fn->ch.c[cidx].t!=VSTR)die("Haris generic: invalid specialization");
            char tmp[256];snprintf(tmp,sizeof tmp,"%s",fn->ch.c[cidx].u.s);char*save=0;for(char*q=strtok_r(tmp,",",&save);q&&ns<16;q=strtok_r(NULL,",",&save))specs[ns++]=xdup(q);
            if(cal.u.fn->ngens!=ns)die("Haris generic: expected %d type arguments, got %d",cal.u.fn->ngens,ns);
        } else {
            ns=cal.u.fn->ngens;
            if(!generic_infer_call(cal.u.fn,n,args2,specs,16))die("Haris generic: unable to infer type arguments; use explicit <T,...>");
        }
        Fn*spfn=generic_specialize(cal.u.fn,specs,ns);for(int si=0;si<ns;si++)xfree((void*)specs[si]);if(!spfn||!generic_validate_call(spfn,n,args2))die("Haris generic: type arguments do not match call");st[sp-n-1].u.fn=spfn;cal=st[sp-n-1];
    }
    if(cal.t==VNATIVE){vm->sp=sp;vm->fp=fp;Value r=cal.u.native(vm,n,args2);sp-=n+1;HPUSH(r);runtime_safe_point(vm,sp,fp);ip++;DISPATCH();}
    if(cal.t==VBOUND){BoundCall*b=cal.u.bound;if(!b||!b->self)die("invalid bound method");int pos=sp-n-1;if(sp>=8191)die("stack overflow");for(int j=n-1;j>=0;j--)st[pos+2+j]=st[pos+1+j];st[pos]=b->fn?(Value){.t=VFN,.u.fn=b->fn}:(Value){.t=VNATIVE,.u.native=b->native};st[pos+1]=*b->self;sp++;n++;args2=&st[sp-n];cal=st[pos];if(b->native){vm->sp=sp;vm->fp=fp;Value r=b->native(vm,n,args2);sp-=n+1;HPUSH(r);runtime_safe_point(vm,sp,fp);ip++;DISPATCH();}}
    if(cal.t==VFN){
        Fn *cf=cal.u.fn;
        if(cf->is_async){Value h=nasync_spawn_fn(vm,cf,n,&st[sp-n]);sp-=n+1;HPUSH(h);ip++;DISPATCH();}
        cf->calls++;
        if(call_ins->cache_kind==1 && call_ins->cache!=cf) call_ins->cache_kind=0;
        if(call_ins->cache_kind==0){call_ins->cache=cf;call_ins->cache_kind=1;}
        (void)jit_maybe_compile(cf);
        if(cf->jit_i64){Value jr;if(jit_call_i64(cf,n,args2,&jr)){sp-=n+1;HPUSH(jr);ip++;DISPATCH();}}
        /* Tail-call elimination: return f(args) reuses the current frame. */
        if(n==cf->np && ip+1<fn->ch.n && code[ip+1].op==I_RET){
            int newbase=base;for(int qi=0;qi<n;qi++)st[newbase+1+qi]=args2[qi];sp=newbase+1+n;fn=cf;code=fn->ch.v;cons=fn->ch.c;ip=0;base=newbase;while(sp<base+1+cf->np+cf->nlocals)sp++,st[sp-1]=vn();DISPATCH();
        }
        if(fp>=H_CALL_MAX)die("call stack overflow");
        frames[fp-1].ip=ip+1;frames[fp++]=(Frame){0,sp-n-1,cf};fn=cf;code=fn->ch.v;cons=fn->ch.c;ip=0;base=sp-n-1;while(sp<base+1+cf->np+cf->nlocals)sp++,st[sp-1]=vn();DISPATCH();
    }
    die("not callable");
}
L_RET: {Value r=HPOP();int oldbase=base;fp--;if(!fp){HRETURN(r);}Frame caller=frames[fp-1];fn=caller.fn;code=fn->ch.v;cons=fn->ch.c;ip=caller.ip;base=caller.base;sp=oldbase;HPUSH(r);DISPATCH();}
L_ARRAY: {int n=code[ip].a;if(n<0||n>sp)die("Haris runtime: invalid array operands");Value v=va();for(int i=0;i<n;i++)ap(v.u.a,st[sp-n+i]);sp-=n;HPUSH(v);ip++;DISPATCH();}
L_INDEX: {if(sp<2)die("Haris runtime: value stack underflow");Value idx=st[--sp],obj=st[--sp];if(obj.t==VSTRUCT&&idx.t==VSTR){HPUSH(stget(obj.u.st,idx.u.s));}else if(idx.t==VINT&&obj.t==VARR){long long k=idx.u.i;if(k<0||(size_t)k>=obj.u.a->n)die("array index out of range");HPUSH(obj.u.a->v[k]);}else if(idx.t==VINT&&obj.t==VSTR){long long k=idx.u.i;if(k<0||(size_t)k>=strlen(obj.u.s))die("string index out of range");char z[2]={obj.u.s[k],0};HPUSH(vs(z));}else die("not indexable");ip++;DISPATCH();}
L_SETINDEX: {if(sp<3)die("Haris runtime: setindex stack underflow");Value val=st[--sp],idx=st[--sp],obj=st[--sp];if(obj.t!=VARR||idx.t!=VINT)die("assignment target must be an array and integer index");long long k=idx.u.i;if(k<0||(size_t)k>=obj.u.a->n)die("array index out of range");obj.u.a->v[k]=val;HPUSH(val);ip++;DISPATCH();}
L_SETFIELD: {if(sp<3)die("Haris runtime: setfield stack underflow");Value val=st[--sp],key=st[--sp],obj=st[--sp];if(obj.t!=VSTRUCT||key.t!=VSTR)die("assignment target must be a struct and field name");stput(obj.u.st,key.u.s,val);HPUSH(val);ip++;DISPATCH();}
L_FIELD: {if(sp<2)die("field stack underflow");Value key=st[--sp],obj=st[--sp];if(key.t!=VSTR||obj.t!=VSTRUCT)die("field access requires struct");Value got=stget(obj.u.st,key.u.s);if(got.t==VNULL)got=gmethod_get(obj,key.u.s);HPUSH(got);ip++;DISPATCH();}
L_STRUCT: {int n=code[ip].a;if(n<0||2*n>sp)die("invalid struct operands");Value v=vsobj();for(int i=0;i<n;i++){Value key=st[sp-2*n+i*2],val=st[sp-2*n+i*2+1];if(key.t!=VSTR)die("struct field name must be string");stput(v.u.st,key.u.s,val);}sp-=2*n;HPUSH(v);ip++;DISPATCH();}
L_TYPECHECK: {if(sp<1||code[ip].a<0||code[ip].a>=fn->ch.nc)die("typecheck stack error");Value exp=fn->ch.c[code[ip].a];if(exp.t!=VSTR||((!(!strcmp(exp.u.s,"float")&&st[sp-1].t==VINT))&&strcmp(type_name(st[sp-1]),exp.u.s)))die("Haris type error: expected %s, got %s",exp.u.s,type_name(st[sp-1]));ip++;DISPATCH();}
L_TRY: if(vm->err_depth>=256)die("Haris runtime: try-handler stack overflow"); vm->err_frames[vm->err_depth]=(struct ErrFrame){code[ip].a,fp,sp}; vm->err_depth++; ip++; DISPATCH();
L_ENDTRY: if(vm->err_depth>0)vm->err_depth--; ip++; DISPATCH();
L_JMPNULL: {if(sp<1)die("Haris runtime: jmpnull stack underflow");if(st[sp-1].t==VNULL){ip=code[ip].a;}else{ip++;}DISPATCH();}
L_HALT: HRETURN(sp?st[sp-1]:vn());
#undef DISPATCH
#else
    /* Portable fallback. The same local-stack optimization is retained. */
    unsigned long long ticks=0; g_runtime_vm=vm;
    int jumped=setjmp(vm->jmp_stack[jmp_slot]);
    if(jumped){
        if(!vm->err_active || vm->err_depth<=0){g_runtime_vm=NULL;die("Haris runtime error: %s",vm->err_msg);}
        vm->err_active=0; if(vm->err_depth<=0){g_runtime_vm=NULL;die("Haris runtime error: %s",vm->err_msg);} struct ErrFrame eh=vm->err_frames[vm->err_depth-1];vm->err_depth--;vm->err_ip=eh.ip;vm->err_fp=eh.fp;vm->err_sp=eh.sp;sp=eh.sp;fp=eh.fp;Frame *ef=&frames[fp-1];ef->ip=eh.ip;HPUSH(vs(vm->err_msg));
    }
    for(;;){if((++ticks & 4095u)==0){runtime_budget_check(vm);if(g_mem_used>=g_gc_next){vm->sp=sp;vm->fp=fp;if(gc_minor(vm)==0 || g_mem_used>=g_gc_next*2)gc_collect(vm);}}Frame*fr=&frames[fp-1];Ins in=fr->fn->ch.v[fr->ip++];switch(in.op){
        case I_CONST:HPUSH(fr->fn->ch.c[in.a]);break;case I_GETL:{int idx=fr->base+1+in.a;if(idx>=sp)die("Haris runtime: invalid local/argument index");HPUSH(st[idx]);break;}case I_SETL:{int idx=fr->base+1+in.a;if(idx<fr->base+1||idx>=sp)die("Haris runtime: invalid local slot");st[idx]=st[--sp];break;}case I_INC_LOCAL:{int idx=fr->base+1+in.a;if(idx<fr->base+1||idx>=sp)die("Haris runtime: invalid local slot");Value v=st[idx];if(v.t==VINT)st[idx]=vi(int_binop(vm,I_ADD,v.u.i,1));else if(v.t==VFLOAT)st[idx]=vf(v.u.f+1.0);else die("numeric operator needs number");break;}case I_DEC_LOCAL:{int idx=fr->base+1+in.a;if(idx<fr->base+1||idx>=sp)die("Haris runtime: invalid local slot");Value v=st[idx];if(v.t==VINT)st[idx]=vi(int_binop(vm,I_SUB,v.u.i,1));else if(v.t==VFLOAT)st[idx]=vf(v.u.f-1.0);else die("numeric operator needs number");break;}case I_LOCAL_CONST_ADD:case I_LOCAL_CONST_SUB:case I_LOCAL_CONST_MUL:case I_LOCAL_CONST_DIV:case I_LOCAL_CONST_MOD:{int idx=fr->base+1+in.a;if(idx<fr->base+1||idx>=sp||in.b<0||in.b>=fr->fn->ch.nc)die("Haris runtime: invalid specialized arithmetic operands");Value v=st[idx],cst=fr->fn->ch.c[in.b];Op op=(Op)in.op;if(v.t==VINT&&cst.t==VINT)st[idx]=vi(int_binop(vm,op==I_LOCAL_CONST_ADD?I_ADD:op==I_LOCAL_CONST_SUB?I_SUB:op==I_LOCAL_CONST_MUL?I_MUL:op==I_LOCAL_CONST_DIV?I_DIV:I_MOD,v.u.i,cst.u.i));else if(isnum(v)&&cst.t==VINT){double y=(double)cst.u.i;Op bop=(op==I_LOCAL_CONST_ADD?I_ADD:op==I_LOCAL_CONST_SUB?I_SUB:op==I_LOCAL_CONST_MUL?I_MUL:op==I_LOCAL_CONST_DIV?I_DIV:I_MOD);if(bop==I_DIV&&y==0.0)die("division by zero");st[idx]=vf(bop==I_ADD?v.u.f+y:bop==I_SUB?v.u.f-y:bop==I_MUL?v.u.f*y:bop==I_DIV?v.u.f/y:fmod(v.u.f,y));}else die("numeric operator needs numbers");break;}case I_LOCAL_BIN:{int di=fr->base+1+in.a,si=fr->base+1+in.b;if(di<fr->base+1||di>=sp||si<fr->base+1||si>=sp)die("Haris runtime: invalid local binary operands");Value a=st[di],b=st[si];Op op=(Op)(uintptr_t)in.cache;if(op<I_ADD||op>I_MOD)die("Haris runtime: invalid local binary opcode");if(a.t==VINT&&b.t==VINT)st[di]=vi(int_binop(vm,op,a.u.i,b.u.i));else{if(!isnum(a)||!isnum(b))die("numeric operator needs numbers");double x=dn(a),y=dn(b);if(op==I_DIV&&y==0.0)die("division by zero");st[di]=vf(op==I_ADD?x+y:op==I_SUB?x-y:op==I_MUL?x*y:op==I_DIV?x/y:fmod(x,y));}break;}case I_LOCAL_CMP_JMPF:{int idx=fr->base+1+in.a,ci=in.b;if(idx<fr->base+1||idx>=sp||ci<0||ci>=fr->fn->ch.nc)die("Haris runtime: invalid local comparison operands");Value a=st[idx],b=fr->fn->ch.c[ci];Op op=(Op)(I_EQ+(int)(in.cache_kind?in.cache_kind-1:0));int r=0;if(op>I_GTE)die("Haris runtime: invalid local comparison opcode");if(a.t==VINT&&b.t==VINT)r=op==I_EQ?a.u.i==b.u.i:op==I_NEQ?a.u.i!=b.u.i:op==I_LT?a.u.i<b.u.i:op==I_LTE?a.u.i<=b.u.i:op==I_GT?a.u.i>b.u.i:a.u.i>=b.u.i;else{if(!isnum(a)||!isnum(b))die("comparison needs numbers");double x=dn(a),y=dn(b);r=op==I_EQ?x==y:op==I_NEQ?x!=y:op==I_LT?x<y:op==I_LTE?x<=y:op==I_GT?x>y:x>=y;}if(!r)fr->ip=(int)(uintptr_t)in.cache;break;}
        case I_GET:{const char*key=fr->fn->ch.names[in.a];if(fr->fn->closure&&env_has_chain(fr->fn->closure,key)){HPUSH(env_get_chain(fr->fn->closure,key));break;}int ci=fr->fn->ch.v[fr->ip-1].line-1;if(ci<0||ci>=vm->g.n||strcmp(vm->g.v[ci].k,key)){ci=egi(&vm->g,key);if(ci<0)die("Haris runtime: undefined '%s'",key);fr->fn->ch.v[fr->ip-1].line=ci+1;}HPUSH(vm->g.v[ci].v);break;}
        case I_SET:{Value v=st[--sp];int ci=fr->fn->ch.v[fr->ip-1].line-1;if(ci<0||ci>=vm->g.n||strcmp(vm->g.v[ci].k,fr->fn->ch.names[in.a]))ci=egi(&vm->g,fr->fn->ch.names[in.a]);if(ci>=0)vm->g.v[ci].v=v;else en(&vm->g,fr->fn->ch.names[in.a],v);if(ci<0)ci=egi(&vm->g,fr->fn->ch.names[in.a]);fr->fn->ch.v[fr->ip-1].line=ci+1;break;}case I_POP:if(sp<=0)die("Haris runtime: value stack underflow");--sp;break;
        case I_NEG:{Value a=HPOP();if(a.t==VINT&&a.u.i==LLONG_MIN){if(vm->module_sandboxed){vm->int_overflow=1;HPUSH(vi(0));}else die("Haris runtime: integer overflow");}else HPUSH(a.t==VINT?vi(-a.u.i):vf(-dn(a)));break;}case I_NOT:{Value a=HPOP();HPUSH(vb(!truth(a)));break;}
        case I_ADD:case I_SUB:case I_MUL:case I_DIV:case I_MOD:{if(sp<2)die("Haris runtime: value stack underflow");Value b=st[--sp],a=st[--sp];if(in.op==I_ADD&&a.t==VSTR&&b.t==VSTR){size_t na=strlen(a.u.s),nb=strlen(b.u.s);char*z=xmalloc(na+nb+1);memcpy(z,a.u.s,na);memcpy(z+na,b.u.s,nb);z[na+nb]=0;HPUSH(((Value){.t=VSTR,.u.s=z}));break;}if(a.t==VINT&&b.t==VINT){HPUSH(vi(int_binop(vm,in.op,a.u.i,b.u.i)));break;}if(!isnum(a)||!isnum(b))die("numeric operator needs numbers");double x=dn(a),y=dn(b);if(in.op==I_DIV&&y==0)die("division by zero");HPUSH(vf(in.op==I_ADD?x+y:in.op==I_SUB?x-y:in.op==I_MUL?x*y:in.op==I_DIV?x/y:fmod(x,y)));break;}
        case I_EQ:case I_NEQ:{if(sp<2)die("Haris runtime: value stack underflow");Value b=st[--sp],a=st[--sp];int r=0;if(a.t==VINT&&b.t==VINT)r=a.u.i==b.u.i;else if(isnum(a)&&isnum(b))r=dn(a)==dn(b);else if(a.t==b.t){if(a.t==VNULL)r=1;else if(a.t==VBOOL)r=a.u.b==b.u.b;else if(a.t==VSTR)r=!strcmp(a.u.s,b.u.s);}if(in.op==I_NEQ)r=!r;HPUSH(vb(r));break;}
        case I_LT:case I_LTE:case I_GT:case I_GTE:{if(sp<2)die("Haris runtime: value stack underflow");Value b=st[--sp],a=st[--sp];if(a.t==VINT&&b.t==VINT){int r=in.op==I_LT?a.u.i<b.u.i:in.op==I_LTE?a.u.i<=b.u.i:in.op==I_GT?a.u.i>b.u.i:a.u.i>=b.u.i;HPUSH(vb(r));break;}if(!isnum(a)||!isnum(b))die("comparison needs numbers");double x=dn(a),y=dn(b);int r=in.op==I_LT?x<y:in.op==I_LTE?x<=y:in.op==I_GT?x>y:x>=y;HPUSH(vb(r));break;}case I_AND:{Value b=st[--sp],a=st[--sp];HPUSH(vb(truth(a)&&truth(b)));break;}case I_OR:{Value b=st[--sp],a=st[--sp];HPUSH(vb(truth(a)||truth(b)));break;}
        case I_JMP:{int target=in.a;if(target<fr->ip-1 && (fr->fn->loop_jit_i64||fr->fn->loop_jit3_i64)){int gi=egi(&vm->g,fr->fn->ch.names[fr->fn->loop_var_name]);Value gv=(gi>=0?vm->g.v[gi].v:vn());if(gv.t==VINT){long long fv=fr->fn->loop_jit3_i64?fr->fn->loop_jit3_i64(gv.u.i,fr->fn->loop_limit,fr->fn->loop_step):fr->fn->loop_jit_i64(gv.u.i,fr->fn->loop_limit);if(gi>=0)vm->g.v[gi].v=vi(fv);else en(&vm->g,fr->fn->ch.names[fr->fn->loop_var_name],vi(fv));fr->ip=fr->fn->loop_exit;break;}}fr->ip=target;break;}case I_JMPF:{Value a=st[--sp];if(!truth(a))fr->ip=in.a;break;}
        case I_MAKE_CLOSURE:{if(sp<1||st[sp-1].t!=VFN)die("Haris runtime: closure expects function");Fn*proto=st[sp-1].u.fn;Fn*c=xmalloc(sizeof(*c));memcpy(c,proto,sizeof(*c));c->closure=xmalloc(sizeof(Env));memset(c->closure,0,sizeof(Env));c->closure->p=fr->fn->closure;if(fr->fn->np+fr->fn->nlocals>0){for(int i=0;i<fr->fn->np+fr->fn->nlocals;i++){const char*k=i<fr->fn->np?fr->fn->params[i]:fr->fn->locals[i-fr->fn->np];int idx=fr->base+1+i;if(idx<sp)en(c->closure,k,st[idx]);}}st[sp-1]=(Value){.t=VFN,.u.fn=c};break;}case I_CALL:{int n=in.a;if(n<0||n>H_CALL_MAX||sp<n+1)die("Haris runtime: stack underflow/invalid argc in call");Ins *call_ins=&in;Value*args2=&st[sp-n];Value cal=st[sp-n-1];if(cal.t==VFN&&cal.u.fn->ngens>0){const char*specs[16]={0};int ns=0;if(in.b>0){int cidx=in.b-1;if(cidx<0||cidx>=fr->fn->ch.nc||fr->fn->ch.c[cidx].t!=VSTR)die("Haris generic: invalid specialization");char tmp[256];snprintf(tmp,sizeof tmp,"%s",fr->fn->ch.c[cidx].u.s);char*save=0;for(char*q=strtok_r(tmp,",",&save);q&&ns<16;q=strtok_r(NULL,",",&save))specs[ns++]=xdup(q);}else{ns=cal.u.fn->ngens;if(!generic_infer_call(cal.u.fn,n,args2,specs,16))die("Haris generic: unable to infer type arguments; use explicit <T,...>");}if(cal.u.fn->ngens!=ns)die("Haris generic: expected %d type arguments, got %d",cal.u.fn->ngens,ns);Fn*spfn=generic_specialize(cal.u.fn,specs,ns);for(int si=0;si<ns;si++)xfree((void*)specs[si]);if(!spfn||!generic_validate_call(spfn,n,args2))die("Haris generic: type arguments do not match call");st[sp-n-1].u.fn=spfn;cal=st[sp-n-1];}if(cal.t==VNATIVE){vm->sp=sp;vm->fp=fp;Value r=cal.u.native(vm,n,args2);sp-=n+1;HPUSH(r);runtime_safe_point(vm,sp,fp);break;}else if(cal.t==VBOUND){BoundCall*b=cal.u.bound;if(!b||!b->self)die("invalid bound method");int pos=sp-n-1;for(int j=n-1;j>=0;j--)st[pos+2+j]=st[pos+1+j];st[pos]=b->fn?(Value){.t=VFN,.u.fn=b->fn}:(Value){.t=VNATIVE,.u.native=b->native};st[pos+1]=*b->self;sp++;n++;args2=&st[sp-n];cal=st[pos];if(b->native){vm->sp=sp;vm->fp=fp;Value r=b->native(vm,n,args2);sp-=n+1;HPUSH(r);runtime_safe_point(vm,sp,fp);break;}}else if(cal.t==VFN){Fn *cf=cal.u.fn;if(cf->is_async){Value h=nasync_spawn_fn(vm,cf,n,args2);sp-=n+1;HPUSH(h);break;}cf->calls++;(void)jit_maybe_compile(cf);Value jr;if(jit_call_i64(cf,n,args2,&jr)){sp-=n+1;HPUSH(jr);break;}if(call_ins->cache_kind==1&&call_ins->cache!=cf)call_ins->cache_kind=0;if(call_ins->cache_kind==0){call_ins->cache=cf;call_ins->cache_kind=1;}if(n==cf->np&&fr->ip<cf->ch.n&&cf->ch.v[fr->ip].op==I_RET){int newbase=fr->base;for(int qi=0;qi<n;qi++)st[newbase+1+qi]=args2[qi];sp=newbase+1+n;fr->fn=cf;fr->ip=0;while(sp<newbase+1+cf->np+cf->nlocals)sp++,st[sp-1]=vn();break;}if(fp>=H_CALL_MAX)die("call stack overflow");frames[fp-1].ip=fr->ip;frames[fp++]=(Frame){0,sp-n-1,cf};while(sp<frames[fp-1].base+1+cf->np+cf->nlocals)sp++,st[sp-1]=vn();}else die("not callable");break;}
        case I_RET:{Value r=HPOP();int b=fr->base;fp--;if(!fp){HRETURN(r);}fr=&frames[fp-1];sp=b;/* caller ip was saved at call */HPUSH(r);break;}
        case I_ARRAY:{int n=in.a;if(n<0||n>sp)die("Haris runtime: invalid array operands");Value v=va();for(int i=0;i<n;i++)ap(v.u.a,st[sp-n+i]);sp-=n;HPUSH(v);break;}
        case I_INDEX:{if(sp<2)die("Haris runtime: value stack underflow");Value idx=st[--sp],obj=st[--sp];if(obj.t==VSTRUCT&&idx.t==VSTR){HPUSH(stget(obj.u.st,idx.u.s));}else if(idx.t==VINT&&obj.t==VARR){long long k=idx.u.i;if(k<0||(size_t)k>=obj.u.a->n)die("array index out of range");HPUSH(obj.u.a->v[k]);}else if(idx.t==VINT&&obj.t==VSTR){long long k=idx.u.i;if(k<0||(size_t)k>=strlen(obj.u.s))die("string index out of range");char z[2]={obj.u.s[k],0};HPUSH(vs(z));}else die("not indexable");break;}case I_SETINDEX:{if(sp<3)die("Haris runtime: setindex stack underflow");Value val=st[--sp],idx=st[--sp],obj=st[--sp];if(obj.t!=VARR||idx.t!=VINT)die("assignment target must be an array and integer index");long long k=idx.u.i;if(k<0||(size_t)k>=obj.u.a->n)die("array index out of range");obj.u.a->v[k]=val;HPUSH(val);break;}case I_FIELD:{if(sp<2)die("field stack underflow");Value key=st[--sp],obj=st[--sp];if(key.t!=VSTR||obj.t!=VSTRUCT)die("field access requires struct");Value got=stget(obj.u.st,key.u.s);if(got.t==VNULL)got=gmethod_get(obj,key.u.s);HPUSH(got);break;}case I_SETFIELD:{if(sp<3)die("Haris runtime: setfield stack underflow");Value val=st[--sp],key=st[--sp],obj=st[--sp];if(obj.t!=VSTRUCT||key.t!=VSTR)die("assignment target must be a struct and field name");stput(obj.u.st,key.u.s,val);HPUSH(val);break;}case I_STRUCT:{int n=in.a;if(n<0||2*n>sp)die("invalid struct operands");Value v=vsobj();for(int i=0;i<n;i++){Value key=st[sp-2*n+i*2],val=st[sp-2*n+i*2+1];if(key.t!=VSTR)die("struct field name must be string");stput(v.u.st,key.u.s,val);}sp-=2*n;HPUSH(v);break;}case I_TYPECHECK:{if(sp<1||in.a<0||in.a>=fr->fn->ch.nc)die("typecheck stack error");Value exp=fr->fn->ch.c[in.a];if(exp.t!=VSTR||((!(!strcmp(exp.u.s,"float")&&st[sp-1].t==VINT))&&strcmp(type_name(st[sp-1]),exp.u.s)))die("Haris type error: expected %s, got %s",exp.u.s,type_name(st[sp-1]));break;}case I_TRY:if(vm->err_depth>=256)die("Haris runtime: try-handler stack overflow");vm->err_frames[vm->err_depth]=(struct ErrFrame){in.a,fp,sp};vm->err_depth++;break;case I_ENDTRY:if(vm->err_depth>0)vm->err_depth--;break;case I_JMPNULL:{if(sp<1)die("Haris runtime: jmpnull stack underflow");if(st[sp-1].t==VNULL){fr->ip=in.a;}break;}case I_HALT:HRETURN(sp?st[sp-1]:vn());default:die("bad opcode");}}
#endif
}

/* Public execution entry point: validation/policy stays here, while the
   bytecode engine itself lives in vm_execute().  This keeps future runtime
   scheduling, tracing, and alternate interpreters out of the opcode loop. */
/* Public execution facade: keep validation here; VM/JIT execution lives in vm_execute(). */
Value run(VM*vm,Fn*f,int argc,Value*args){
    if(!vm||!f||argc<0||argc>H_CALL_MAX) return vn();
    if(argc && !args) return vn();
    return vm_execute(vm,f,argc,args);
}

static void jit_shutdown(void){
    if(g_web_multi){curl_multi_cleanup(g_web_multi);g_web_multi=NULL;}
    JitBlock*b=g_jit_blocks;while(b){JitBlock*n=b->next;jit_free(b->p,b->n);free(b);b=n;}g_jit_blocks=NULL;
}
static char* http_text(const char*url,size_t*len);
static int package_search(const char*q){const char*base=getenv("HARIS_REGISTRY");if(!base||!*base)base="https://registry.haris.dev/index.tsv";size_t len=0;char*idx=http_text(base,&len);if(!idx){fprintf(stderr,"haris: registry unavailable\n");return 2;}int hits=0;char*save=0;for(char*line=strtok_r(idx,"\n",&save);line;line=strtok_r(NULL,"\n",&save)){char*copy=xdup(line);char*n=strtok(copy,"\t"),*v=strtok(NULL,"\t"),*u=strtok(NULL,"\t");if(n&&v&&(!q||!*q||strcasestr(line,q))){printf("%s\t%s",n,v);if(u)printf("\t%s",u);putchar('\n');hits++;}xfree(copy);}xfree(idx);return hits?0:3;}


static int sha256_mem_hex(const unsigned char *data,size_t n,char out[65]){
    EVP_MD_CTX*c=EVP_MD_CTX_new();if(!c)return 0;unsigned char h[EVP_MAX_MD_SIZE];unsigned int hn=0;
    int ok=EVP_DigestInit_ex(c,EVP_sha256(),NULL)==1&&EVP_DigestUpdate(c,data,n)==1&&EVP_DigestFinal_ex(c,h,&hn)==1&&hn==32;
    EVP_MD_CTX_free(c);if(!ok)return 0;for(unsigned int i=0;i<hn;i++)sprintf(out+i*2,"%02x",h[i]);out[64]=0;return 1;
}
static int haris_self_path(char*out,size_t cap,const char*argv0){
    if(!out||cap<2)return 0;
#ifdef _WIN32
    DWORD n=GetModuleFileNameA(NULL,out,(DWORD)cap);if(n>0&&n<cap)return 1;
#elif defined(__APPLE__)
    uint32_t n=(uint32_t)cap;if(_NSGetExecutablePath(out,&n)==0)return 1;
    if(n>0){char *tmp=(char*)xmalloc((size_t)n+1);uint32_t m=n+1;if(_NSGetExecutablePath(tmp,&m)==0){snprintf(out,cap,"%s",tmp);xfree(tmp);return 1;}xfree(tmp);}
#elif defined(__linux__)
    ssize_t n=readlink("/proc/self/exe",out,cap-1);if(n>0){out[n]=0;return 1;}
#endif
    if(argv0&&*argv0){
#ifdef _WIN32
        if(_fullpath(out,argv0,cap))return 1;
#else
        if(realpath(argv0,out))return 1;
#endif
        if(strlen(argv0)<cap){snprintf(out,cap,"%s",argv0);return 1;}
    }
    return 0;
}
static uint64_t read_le64(const unsigned char*p){uint64_t x=0;for(int i=7;i>=0;i--)x=(x<<8)|p[i];return x;}
static void write_le64(unsigned char*p,uint64_t x){for(int i=0;i<8;i++){p[i]=(unsigned char)(x&255u);x>>=8;}}
static uint32_t read_le32(const unsigned char*p){uint32_t x=0;for(int i=3;i>=0;i--)x=(x<<8)|p[i];return x;}
static void write_le32(unsigned char*p,uint32_t x){for(int i=0;i<4;i++){p[i]=(unsigned char)(x&255u);x>>=8;}}
static int haris_pack_archive_put(unsigned char **buf,size_t *n,size_t *cap,const void *src,size_t len){
    if(!buf||!n||!cap||(!src&&len)||*n>HARIS_PACK_MAX||len>HARIS_PACK_MAX-*n)return 0;
    size_t need=*n+len;
    if(need>*cap){
        size_t nc=*cap?*cap:4096u;
        while(nc<need){
            if(nc>=HARIS_PACK_MAX/2u){nc=HARIS_PACK_MAX;break;}
            nc*=2u;
        }
        if(nc<need)nc=need;
        if(nc>HARIS_PACK_MAX)nc=HARIS_PACK_MAX;
        unsigned char*q=(unsigned char*)realloc(*buf,nc);
        if(!q)return 0;
        *buf=q;*cap=nc;
    }
    if(len)memcpy(*buf+*n,src,len);
    *n=need;
    return 1;
}
static int haris_builtin_module_name(const char*m){
    static const char*const a[]={"os","system","ai","sql","games","game","net","web","cloud","git","defense","logs","data","nuclear","engine","async","gpu","cpu","fs","file","dir","path","map","set","queue","stack","tuple","range","random","string","math","memory","regex","datetime","gfx","json","security","api"};
    for(size_t i=0;i<sizeof(a)/sizeof(a[0]);i++)if(!strcmp(m,a[i]))return 1;return 0;
}
static int haris_pack_relpath(const char*abs,char*out,size_t cap){
    char cwd[PATH_MAX]={0},realp[PATH_MAX]={0};
    if(!abs||!out||!cap||!path_real_abs(abs,realp,sizeof realp)||!path_real_abs(".",cwd,sizeof cwd))return 0;
#ifdef _WIN32
    size_t n=strlen(cwd);
    while(n>3&&(cwd[n-1]=='/'||cwd[n-1]=='\\'))n--;
    int cwd_root=(n==3&&cwd[1]==':'&&(cwd[2]=='/'||cwd[2]=='\\'));
    if(_strnicmp(realp,cwd,n)!=0)return 0;
#else
    size_t n=strlen(cwd);
    while(n>1&&cwd[n-1]=='/')n--;
    int cwd_root=(n==1&&cwd[0]=='/');
    if(strncmp(realp,cwd,n)!=0)return 0;
#endif
    if(!cwd_root&&realp[n]&&realp[n]!='/'&&realp[n]!='\\')return 0;
    const char*r=realp+n;
    while(*r=='/'||*r=='\\')r++;
    if(!*r)return 0;
    if(strlen(r)+1>cap)return 0;
    for(char*p=(char*)r;*p;p++)if(*p=='\\')*p='/';
    if(!haris_pack_path_safe(r))return 0;
    snprintf(out,cap,"%s",r);
    return 1;
}
typedef struct {char *path;char *src;size_t n;} HPackBuildEntry;
typedef struct {HPackBuildEntry v[HARIS_PACK_MAX_ENTRIES];size_t n,total;} HPackBuild;
static int haris_pack_collect_imports(HPackBuild*b,VM*resolver,const char*src,int depth);
static int haris_pack_collect_imports(HPackBuild*b,VM*resolver,const char*src,int depth){
    if(!b||!resolver||!src||depth>64)return 0;
    TV tv=lex(src);
    for(int i=0;i<tv.n;i++){
        const char*m=NULL;
        if(tv.v[i].t==TIMPORT||tv.v[i].t==TFROM){
            int j=i+1;
            while(j<tv.n&&tv.v[j].t==TNL)j++;
            if(j<tv.n&&(tv.v[j].t==TID||tv.v[j].t==TSTR))m=tv.v[j].s;
        }
        if(!m||haris_builtin_module_name(m))continue;
        char path[PATH_MAX]={0};char*dep=NULL;
        if(!module_resolve(resolver,m,path,sizeof path,&dep)){
            fprintf(stderr,"haris pack: unresolved imported module '%s'\n",m);
            return 0;
        }
        char logical[PATH_MAX]={0};
        if(!haris_pack_relpath(path,logical,sizeof logical)){
            fprintf(stderr,"haris pack: imported module '%s' is outside the project root\n",m);
            xfree(dep);
            return 0;
        }
        int seen=0;
        for(size_t z=0;z<b->n;z++)if(!strcmp(b->v[z].path,logical)){seen=1;break;}
        if(seen){xfree(dep);continue;}
        size_t dep_n=strlen(dep);
        if(dep_n==0||dep_n>H_SOURCE_MAX){
            fprintf(stderr,"haris pack: imported module '%s' exceeds the per-file source limit\n",m);
            xfree(dep);
            return 0;
        }
        if(b->n>=HARIS_PACK_MAX_ENTRIES||dep_n>HARIS_PACK_MAX_TOTAL-b->total){
            fprintf(stderr,"haris pack: package exceeds the %llu-byte total source limit\n",
                    (unsigned long long)HARIS_PACK_MAX_TOTAL);
            xfree(dep);
            return 0;
        }
        b->v[b->n].path=xdup(logical);
        b->v[b->n].src=xdup(dep);
        b->v[b->n].n=dep_n;
        b->n++;
        b->total+=dep_n;
        if(!haris_pack_collect_imports(b,resolver,dep,depth+1)){xfree(dep);return 0;}
        xfree(dep);
    }
    return 1;
}
static int haris_pack_build_archive(const char*main_src,HPackBuild*b,unsigned char**out,size_t*outn){
    if(!main_src||!b||!out||!outn||b->n==0||b->n>HARIS_PACK_MAX_ENTRIES||b->total>HARIS_PACK_MAX_TOTAL)return 0;*out=NULL;*outn=0;
    if(!haris_pack_path_safe("__main__.hr"))return 0;
    /* caller has already populated b[0] with __main__.hr */
    unsigned char *buf=NULL;size_t n=0,cap=0;unsigned char h[HARIS_ARCH_HEADER_SIZE];memcpy(h,HARIS_ARCH_MAGIC,HARIS_ARCH_MAGIC_LEN);write_le32(h+HARIS_ARCH_MAGIC_LEN,HARIS_ARCH_VERSION);write_le32(h+HARIS_ARCH_MAGIC_LEN+4u,(uint32_t)b->n);
    if(!haris_pack_archive_put(&buf,&n,&cap,h,sizeof h)){free(buf);return 0;}
    for(size_t i=0;i<b->n;i++){
        size_t pn=strlen(b->v[i].path);if(pn==0||pn>=PATH_MAX||b->v[i].n==0||b->v[i].n>H_SOURCE_MAX){free(buf);return 0;}
        unsigned char eh[12];write_le32(eh,(uint32_t)pn);write_le64(eh+4,(uint64_t)b->v[i].n);if(!haris_pack_archive_put(&buf,&n,&cap,eh,sizeof eh)||!haris_pack_archive_put(&buf,&n,&cap,b->v[i].path,pn)||!haris_pack_archive_put(&buf,&n,&cap,b->v[i].src,b->v[i].n)){free(buf);return 0;}
    }
    *out=buf;*outn=n;return 1;
}
static int haris_native_image_valid(FILE*f){
    unsigned char h[4]={0};
    if(!f||fseek(f,0,SEEK_SET)!=0)return 0;
    if(fread(h,1,sizeof h,f)!=sizeof h)return 0;
#ifdef _WIN32
    return h[0]=='M'&&h[1]=='Z';
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    return h[0]==0x7f&&h[1]=='E'&&h[2]=='L'&&h[3]=='F';
#elif defined(__APPLE__)
    {
        uint32_t m=0;memcpy(&m,h,sizeof m);
        return m==0xFEEDFACEu||m==0xFEEDFACFu||m==0xCEFAEDFEu||m==0xCFFAEDFEu||
               m==0xCAFEBABEu||m==0xBEBAFECAu||m==0xCAFEBABFu||m==0xBFBAFECAu;
    }
#else
    return 1;
#endif
}

static int haris_pack_footer_basic(const unsigned char*hdr,uint32_t*version,uint32_t*flags,uint64_t*archive_size){
    if(memcmp(hdr,HARIS_PACK_MAGIC,HARIS_PACK_MAGIC_LEN)!=0)return 0;
    if(version)*version=read_le32(hdr+HARIS_PACK_VERSION_OFF);
    if(flags)*flags=read_le32(hdr+HARIS_PACK_FLAGS_OFF);
    if(archive_size)*archive_size=read_le64(hdr+HARIS_PACK_ARCHIVE_SIZE_OFF);
    return 1;
}

static int haris_pack_footer_validate(const unsigned char*hdr,uint64_t fs,uint64_t*archive_size){
    uint32_t version=0,flags=0;uint64_t an=0;
    if(!haris_pack_footer_basic(hdr,&version,&flags,&an))return 0;
    if(version!=HARIS_PACK_VERSION||an==0||an>HARIS_PACK_MAX_ARCHIVE)return 0;
    if(flags&~HARIS_PACK_KNOWN_FLAGS)return 0;
    if(fs<(uint64_t)HARIS_PACK_FOOTER_SIZE+an+(uint64_t)HARIS_PACK_HEADER_SIZE)return 0;
    *archive_size=an;
    return 1;
}

static int haris_pack_header_matches_footer(const unsigned char*header,const unsigned char*footer){
    if(memcmp(header,footer,HARIS_PACK_MAGIC_LEN)!=0)return 0;
    if(read_le32(header+HARIS_PACK_VERSION_OFF)!=read_le32(footer+HARIS_PACK_VERSION_OFF))return 0;
    if(read_le32(header+HARIS_PACK_FLAGS_OFF)!=read_le32(footer+HARIS_PACK_FLAGS_OFF))return 0;
    if(read_le64(header+HARIS_PACK_ARCHIVE_SIZE_OFF)!=read_le64(footer+HARIS_PACK_ARCHIVE_SIZE_OFF))return 0;
    return memcmp(header+HARIS_PACK_SHA_OFF,footer+HARIS_PACK_SHA_OFF,64)==0;
}

static int haris_read_packed(const char*exe,char**out,size_t*olen){
    if(!exe||!out||!olen)return 0;
    *out=NULL;*olen=0;
    FILE*f=fopen(exe,"rb");
    if(!f)return 0;
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return 0;}
    long long fsl=ftell(f);
    if(fsl<0){fclose(f);return 0;}
    uint64_t fs=(uint64_t)fsl;
    if(fs<(uint64_t)HARIS_PACK_FOOTER_SIZE){fclose(f);return 0;}

    if(fseek(f,(long)(fs-(uint64_t)HARIS_PACK_FOOTER_SIZE),SEEK_SET)!=0){fclose(f);return 0;}
    unsigned char footer[HARIS_PACK_FOOTER_SIZE];
    if(fread(footer,1,sizeof footer,f)!=sizeof footer){fclose(f);return 0;}

    /*
       Recognize a package only when both the footer and its matching header
       exist. A normal executable ending with the magic alone is not a pack.
    */
    uint64_t an=0;
    int fv=haris_pack_footer_validate(footer,fs,&an);
    if(fv==0){fclose(f);return 0;}
    if(fv<0){fclose(f);return HARIS_PACK_INVALID;}

    uint64_t header_off=fs-(uint64_t)HARIS_PACK_FOOTER_SIZE-an-(uint64_t)HARIS_PACK_HEADER_SIZE;
    if(header_off>(uint64_t)LONG_MAX){fclose(f);return HARIS_PACK_INVALID;}
    if(fseek(f,(long)header_off,SEEK_SET)!=0){fclose(f);return HARIS_PACK_INVALID;}
    unsigned char header[HARIS_PACK_HEADER_SIZE];
    if(fread(header,1,sizeof header,f)!=sizeof header){fclose(f);return HARIS_PACK_INVALID;}
    if(!haris_pack_header_matches_footer(header,footer)){fclose(f);return 0;}
    if(!haris_native_image_valid(f)){fclose(f);return HARIS_PACK_INVALID;}

    uint32_t flags=read_le32(header+HARIS_PACK_FLAGS_OFF);
    if(flags&~HARIS_PACK_KNOWN_FLAGS){fclose(f);return HARIS_PACK_INVALID;}

    uint64_t archive_off=fs-(uint64_t)HARIS_PACK_FOOTER_SIZE-an;
    if(archive_off>(uint64_t)LONG_MAX){fclose(f);return HARIS_PACK_INVALID;}
    if(fseek(f,(long)archive_off,SEEK_SET)!=0){fclose(f);return HARIS_PACK_INVALID;}

    unsigned char*archive=(unsigned char*)malloc((size_t)an);
    if(!archive){fclose(f);return HARIS_PACK_INVALID;}
    if(fread(archive,1,(size_t)an,f)!=(size_t)an){free(archive);fclose(f);return HARIS_PACK_INVALID;}
    fclose(f);

    char sha[65];
    if(!sha256_mem_hex(archive,(size_t)an,sha)||memcmp(sha,footer+HARIS_PACK_SHA_OFF,64)!=0){
        free(archive);return HARIS_PACK_INVALID;
    }
    if((size_t)an<HARIS_ARCH_HEADER_SIZE||
       memcmp(archive,HARIS_ARCH_MAGIC,HARIS_ARCH_MAGIC_LEN)!=0||
       read_le32(archive+HARIS_ARCH_MAGIC_LEN)!=HARIS_ARCH_VERSION){
        free(archive);return HARIS_PACK_INVALID;
    }

    uint32_t count=read_le32(archive+HARIS_ARCH_MAGIC_LEN+4u);
    if(count==0||count>HARIS_PACK_MAX_ENTRIES){free(archive);return HARIS_PACK_INVALID;}

    haris_pack_runtime_clear();
    g_pack_entries=(HPackEntry*)calloc(count,sizeof(HPackEntry));
    if(!g_pack_entries){free(archive);return HARIS_PACK_INVALID;}
    g_pack_entry_n=0;

    size_t pos=HARIS_ARCH_HEADER_SIZE;
    size_t total_source=0;
    for(uint32_t i=0;i<count;i++){
        if(pos>an||an-pos<12u){haris_pack_runtime_clear();free(archive);return HARIS_PACK_INVALID;}
        uint32_t pn=read_le32(archive+pos);
        uint64_t dn=read_le64(archive+pos+4u);
        pos+=12u;
        if(pn==0||pn>=PATH_MAX||dn==0||dn>H_SOURCE_MAX||
           pn>(uint64_t)(an-pos)||dn>(uint64_t)(an-pos-pn)||
           dn>HARIS_PACK_MAX_TOTAL-total_source){
            haris_pack_runtime_clear();free(archive);return HARIS_PACK_INVALID;
        }
        char *path=(char*)malloc((size_t)pn+1);
        char *src=(char*)malloc((size_t)dn+1);
        if(!path||!src){free(path);free(src);haris_pack_runtime_clear();free(archive);return HARIS_PACK_INVALID;}
        memcpy(path,archive+pos,pn);path[pn]=0;pos+=(size_t)pn;
        memcpy(src,archive+pos,(size_t)dn);src[dn]=0;pos+=(size_t)dn;
        if(!haris_pack_path_safe(path)||haris_pack_find(path)>=0){
            free(path);free(src);haris_pack_runtime_clear();free(archive);return HARIS_PACK_INVALID;
        }
        total_source+=(size_t)dn;
        g_pack_entries[i]=(HPackEntry){path,src,(size_t)dn};
        g_pack_entry_n++;
    }
    if(pos!=(size_t)an){haris_pack_runtime_clear();free(archive);return HARIS_PACK_INVALID;}

    int mi=haris_pack_find("__main__.hr");
    if(mi<0){haris_pack_runtime_clear();free(archive);return HARIS_PACK_INVALID;}
    g_pack_loaded=1;
    *olen=g_pack_entries[mi].n;
    *out=xmalloc(g_pack_entries[mi].n+1);
    memcpy(*out,g_pack_entries[mi].src,g_pack_entries[mi].n+1);
    free(archive);
    return 1;
}
static const char*haris_pack_default_output(const char*src,char*out,size_t cap){
    if(!src||!out||cap<8)return NULL;const char*base=strrchr(src,'/');
#ifdef _WIN32
    const char*b2=strrchr(src,'\\');if(!base||b2>base)base=b2;
#endif
    base=base?base+1:src;size_t n=strlen(base);if(n>3&&!strcasecmp(base+n-3,".hr"))n-=3;
#ifdef _WIN32
    snprintf(out,cap,"%.*s.exe",(int)n,base);
#elif defined(__APPLE__)
    snprintf(out,cap,"%.*s_mac",(int)n,base);
#else
    snprintf(out,cap,"%.*s",(int)n,base);
#endif
    return out;
}
static int haris_same_file(const char*a,const char*b){
    if(!a||!b)return 0;
#ifdef _WIN32
    HANDLE ha=CreateFileA(a,0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,
                          FILE_FLAG_BACKUP_SEMANTICS,NULL);
    HANDLE hb=CreateFileA(b,0,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,
                          FILE_FLAG_BACKUP_SEMANTICS,NULL);
    if(ha!=INVALID_HANDLE_VALUE&&hb!=INVALID_HANDLE_VALUE){
        BY_HANDLE_FILE_INFORMATION ia,ib;
        int ok=GetFileInformationByHandle(ha,&ia)&&GetFileInformationByHandle(hb,&ib);
        CloseHandle(ha);CloseHandle(hb);
        if(ok){
            return ia.dwVolumeSerialNumber==ib.dwVolumeSerialNumber&&
                   ia.nFileIndexHigh==ib.nFileIndexHigh&&
                   ia.nFileIndexLow==ib.nFileIndexLow;
        }
    } else {
        if(ha!=INVALID_HANDLE_VALUE)CloseHandle(ha);
        if(hb!=INVALID_HANDLE_VALUE)CloseHandle(hb);
    }
    char aa[PATH_MAX],bb[PATH_MAX];
    DWORD na=GetFullPathNameA(a,sizeof aa,aa,NULL),nb=GetFullPathNameA(b,sizeof bb,bb,NULL);
    if(na&&nb)return _stricmp(aa,bb)==0;
#else
    struct stat sa,sb;
    if(stat(a,&sa)==0&&stat(b,&sb)==0){
        if(sa.st_dev==sb.st_dev&&sa.st_ino==sb.st_ino)return 1;
    }
    char aa[PATH_MAX],bb[PATH_MAX];
    if(realpath(a,aa)&&realpath(b,bb))return !strcmp(aa,bb);
#endif
    return !strcmp(a,b);
}
static void haris_pack_build_free(HPackBuild*b){if(!b)return;for(size_t i=0;i<b->n;i++){xfree(b->v[i].path);xfree(b->v[i].src);}memset(b,0,sizeof *b);}
static int haris_pack_file(const char*src,const char*out,const char*self_exe){
    if(!src||!out||!self_exe)return 2;
    if(haris_same_file(src,out)||haris_same_file(self_exe,out)){
        fprintf(stderr,"haris pack: output must not replace input or active runtime\n");
        return 2;
    }
    char*main_src=readf_limit(src,H_SOURCE_MAX);
    if(!main_src){fprintf(stderr,"haris pack: cannot read '%s'\n",src);return 2;}
    size_t main_n=strlen(main_src);
    if(main_n==0||main_n>H_SOURCE_MAX||main_n>HARIS_PACK_MAX_TOTAL){
        xfree(main_src);
        fprintf(stderr,"haris pack: main source exceeds package limits\n");
        return 2;
    }
    HPackBuild b={0};
    b.v[0]=(HPackBuildEntry){xdup("__main__.hr"),xdup(main_src),main_n};
    b.n=1;b.total=main_n;
    VM resolver;init(&resolver);
    int ok=haris_pack_collect_imports(&b,&resolver,main_src,0);if(!ok){haris_pack_build_free(&b);xfree(main_src);return 2;}
    unsigned char*archive=NULL;size_t an=0;if(!haris_pack_build_archive(main_src,&b,&archive,&an)){haris_pack_build_free(&b);xfree(main_src);return 2;}
    char sha[65];if(!sha256_mem_hex(archive,an,sha)){free(archive);haris_pack_build_free(&b);xfree(main_src);return 2;}
    FILE*in=fopen(self_exe,"rb");FILE*o=fopen(out,"wb");if(!in||!o){if(in)fclose(in);if(o)fclose(o);free(archive);haris_pack_build_free(&b);xfree(main_src);fprintf(stderr,"haris pack: cannot create '%s'\n",out);return 2;}
    unsigned char buf[65536];size_t nr;int wrok=1;while((nr=fread(buf,1,sizeof buf,in))>0){if(fwrite(buf,1,nr,o)!=nr){wrok=0;break;}}if(ferror(in))wrok=0;
    unsigned char header[HARIS_PACK_HEADER_SIZE],footer[HARIS_PACK_FOOTER_SIZE];
    memset(header,0,sizeof header);
    memcpy(header,HARIS_PACK_MAGIC,HARIS_PACK_MAGIC_LEN);
    write_le32(header+HARIS_PACK_VERSION_OFF,HARIS_PACK_VERSION);
    write_le32(header+HARIS_PACK_FLAGS_OFF,HARIS_PACK_FLAGS);
    write_le64(header+HARIS_PACK_ARCHIVE_SIZE_OFF,(uint64_t)an);
    memcpy(header+HARIS_PACK_SHA_OFF,sha,64);
    memcpy(footer,header,sizeof footer);
    if(wrok&&(
       fwrite(header,1,sizeof header,o)!=sizeof header||
       fwrite(archive,1,an,o)!=an||
       fwrite(footer,1,sizeof footer,o)!=sizeof footer)){wrok=0;}
    size_t packed_count=b.n;size_t packed_total=b.total;
    if(wrok&&fflush(o)!=0)wrok=0;
    if(fclose(in)!=0)wrok=0;
    if(fclose(o)!=0)wrok=0;
    free(archive);haris_pack_build_free(&b);xfree(main_src);
    if(!wrok){remove(out);fprintf(stderr,"haris pack: write failed for '%s'\n",out);return 2;}
#ifndef _WIN32
    struct stat st;if(stat(self_exe,&st)==0)chmod(out,st.st_mode&07777);
#endif
    printf("Packed %s -> %s (standalone, %zu .hr files, %.1f MiB source payload)\n",
           src,out,packed_count,(double)packed_total/(1024.0*1024.0));
    puts("Warning: haris pack does not encrypt embedded .hr source; the payload can be extracted from the package.");
    return 0;
}
static int cmd_pack(const char*src,const char*out,const char*argv0){char self[PATH_MAX]={0},dst[PATH_MAX]={0};if(!haris_self_path(self,sizeof self,argv0))return 2;snprintf(dst,sizeof dst,"%s",out&&*out?out:"");if(!*dst)haris_pack_default_output(src,dst,sizeof dst);return haris_pack_file(src,dst,self);}

static int cmd_arena_demo(void){
    VM vm; init(&vm);
    puts("Haris Arena demo v" HARIS_VERSION);
    Value rs=game_room_server_create(&vm,2,(Value[]){vi(8),vi(8)}); if(rs.t!=VHANDLE)return 2;
    int net_ok=0; Value srvudp=udp_bind(&vm,2,(Value[]){vs("127.0.0.1"),vi(0)});
    if(srvudp.t==VHANDLE){Value ui=udp_info(&vm,1,&srvudp);Value pv=stget(ui.u.st,"port");if(pv.t==VINT&&pv.u.i>0){Value cliudp=udp_open(&vm,3,(Value[]){vs("127.0.0.1"),pv,vb(1)});if(cliudp.t==VHANDLE){Value rels=net_reliable_open(&vm,1,&srvudp),relc=net_reliable_open(&vm,1,&cliudp);if(rels.t==VHANDLE&&relc.t==VHANDLE){Value sent=net_reliable_send(&vm,2,(Value[]){relc,vs("arena-ping")});Value got=net_reliable_poll(&vm,2,(Value[]){rels,vi(1000)});net_ok=(sent.t==VINT&&sent.u.i>0&&got.t==VSTRUCT);net_reliable_close(&vm,1,&relc);net_reliable_close(&vm,1,&rels);}udp_close(&vm,1,&cliudp);}}udp_close(&vm,1,&srvudp);}
    int ok=game_room_create(&vm,2,(Value[]){rs,vs("arena")}).u.b;
    ok=ok&&game_room_join(&vm,3,(Value[]){rs,vs("arena"),vs("alice")}).u.b;
    ok=ok&&game_room_join(&vm,3,(Value[]){rs,vs("arena"),vs("bob")}).u.b;
    Value mm=game_matchmaker_create(&vm,2,(Value[]){vi(4),vi(2)}); if(mm.t!=VHANDLE)ok=0;
    if(ok){game_matchmaker_enqueue(&vm,3,(Value[]){mm,vs("alice"),vf(1200)});game_matchmaker_enqueue(&vm,3,(Value[]){mm,vs("bob"),vf(1190)});game_matchmaker_enqueue(&vm,3,(Value[]){mm,vs("carol"),vf(1210)});game_matchmaker_enqueue(&vm,3,(Value[]){mm,vs("dave"),vf(1185)});}
    Value matches=ok?game_matchmaker_tick(&vm,1,&mm):vn();
    Value tx=game_replication_create(&vm,0,NULL),rx=game_replication_create(&vm,0,NULL);
    Value snap=(tx.t==VHANDLE)?game_replication_snapshot(&vm,2,(Value[]){tx,vs("tick=42;players=4") }):vn();
    Value applied=(rx.t==VHANDLE&&snap.t==VSTRUCT)?game_replication_apply(&vm,2,(Value[]){rx,snap}):vn();
    Value ac=game_anticheat_create(&vm,0,NULL);
    Value mv=(ac.t==VHANDLE)?game_anticheat_validate_move(&vm,6,(Value[]){ac,vs("alice"),vf(0),vf(0),vf(0),vf(0.016)}):vb(0);
    printf("room=%s players=%lld match=%s replicated=%s anti_cheat=%s\n",
        ok?"ok":"fail",ok?(long long)game_room_players(&vm,2,(Value[]){rs,vs("arena")}).u.a->n:0,
        matches.t==VARR&&matches.u.a->n?"ok":"pending",applied.t==VSTRUCT?"ok":"fail",mv.t==VBOOL&&mv.u.b?"ok":"fail");
    printf("reliable_udp=%s\n",net_ok?"ok":"fail");
    if(mm.t==VHANDLE)game_matchmaker_close(&vm,1,&mm); if(tx.t==VHANDLE)game_replication_close(&vm,1,&tx);if(rx.t==VHANDLE)game_replication_close(&vm,1,&rx);if(ac.t==VHANDLE)game_anticheat_close(&vm,1,&ac);game_room_server_destroy(&vm,1,&rs);
    return (ok&&net_ok)?0:2;
}

static uint64_t bench_now_us(void){
#ifdef _WIN32
    static LARGE_INTEGER freq; static int initf=0; LARGE_INTEGER c; if(!initf){QueryPerformanceFrequency(&freq);initf=1;} QueryPerformanceCounter(&c); return (uint64_t)((c.QuadPart*1000000LL)/freq.QuadPart);
#else
    struct timespec ts; if(clock_gettime(CLOCK_MONOTONIC,&ts)!=0)return 0; return (uint64_t)ts.tv_sec*1000000ULL+(uint64_t)ts.tv_nsec/1000ULL;
#endif
}
static int cmd_bench(int argc,char**argv){
    int iters=1000;if(argc>0){long x=strtol(argv[0],NULL,10);if(x>0&&x<=1000000)iters=(int)x;}
    VM vm;init(&vm);
    uint64_t t0=bench_now_us();long long acc=0;for(int i=0;i<iters;i++)acc+=i*3LL+7;uint64_t t1=bench_now_us();
    Value tx=game_replication_create(&vm,0,NULL);uint64_t t2=bench_now_us();for(int i=0;i<iters;i++){char b[64];snprintf(b,sizeof b,"player=%d;tick=%d;hp=%d",i%64,i,i%100);Value z=game_replication_snapshot(&vm,2,(Value[]){tx,vs(b)});if(z.t!=VSTRUCT){game_replication_close(&vm,1,&tx);return 2;}}uint64_t t3=bench_now_us();game_replication_close(&vm,1,&tx);
    Value mm=game_matchmaker_create(&vm,2,(Value[]){vi(4),vi(2)});for(int i=0;i<4;i++){char id[32];snprintf(id,sizeof id,"bench-p%d",i);game_matchmaker_enqueue(&vm,3,(Value[]){mm,vs(id),vf(100+i)});}uint64_t t4=bench_now_us();Value m=game_matchmaker_tick(&vm,1,&mm);uint64_t t5=bench_now_us();game_matchmaker_close(&vm,1,&mm);
    printf("bench iterations=%d arithmetic_us=%llu replication_us=%llu matchmaker_us=%llu checksum=%lld matches=%zu\n",iters,(unsigned long long)(t1-t0),(unsigned long long)(t3-t2),(unsigned long long)(t5-t4),acc,(m.t==VARR?m.u.a->n:0));return 0;
}

static void usage(void){
    printf("Haris v%s\n",HARIS_VERSION);
    puts("Usage:");
    puts("  haris [options] <file.hr>");
    puts("  haris -e \"code\"");
    puts("  haris repl");
    puts("  haris init [name]");
    puts("  haris add <package>");
    puts("  haris install <package>");
    puts("  haris remove <package>");
    puts("  haris uninstall <package>");
    puts("  haris update"); 
    puts("  haris pack <main.hr> [output]");
    puts("      Creates a standalone executable; default output is .exe on Windows.");
    puts("      Pack does NOT encrypt embedded .hr source; use encryption and/or native AOT for IP protection.");

    puts("  haris search <query>");
    puts("  haris test [path]");
    puts("  haris arena demo");
    puts("  haris bench [iterations]");
    puts("  haris debug <file.hr>");
    puts("");
    puts("Options:");
    puts("  -h, --help          Show this help");
    puts("  --version           Show version");
    puts("  --max-cpu-ms N      CPU time limit");
    puts("  --max-memory-mb N   RAM limit");
    puts("  --cpu-cores N       CPU core affinity");
    puts("  --cpu-percent N     CPU target percent");
    puts("  --profile NAME      Resource profile");
    puts("  --allow-module-cap C Grant C to imported .hr modules (repeatable)");
    puts("  --trust-module NAME  Trust a module name/path for explicitly requested privileged capabilities");
    puts("");
    puts("AI: Xavier initialization, Adam training, bounded batch inference, embeddings, RAG, GGUF/LLM, tensors and agents.");
    puts("AI Systems: real CUDA Driver execution, ONNX common-graph import/export/run, Transformer encoder blocks, CNN conv2d, RNN/LSTM, model zoo, Hugging Face Hub, TCP distributed all-reduce, FP16/BF16 mixed precision and graph fusion.");
    puts("Network: native HTTPS/TLS 1.2+ server, dynamic HTTPS web apps, optional native HTTP/3 over QUIC + nghttp3, and optional WebRTC via libdatachannel.");
    puts("Security: imported .hr libraries run isolated with capability allowlists, root-confined paths and inherited CPU/RAM limits; untrusted libraries cannot receive system/nuclear/raw-hardware capabilities.");
    puts("Library: create modules/<name>.hr or packages/<name>/__init__.hr, mark public symbols with 'export', then use: import <name> or from <name> import symbol.");
    puts("Library capabilities: add '# @capabilities: fs, web' near the top; the host must also grant each capability with --allow-module-cap.");
    puts("Haris Forge: Game Engine Runtime + Game Networking + High-Performance Web + HTTPS + HTTP/3 + WebRTC + UDP + WebSocket + Cloud + AI + Data Analytics + Security.");
    puts("Arena: reliable UDP + rooms/lobbies + matchmaking + replication + server-authoritative anti-cheat.");
    puts("Development: built-in benchmark suite, package-manager regression tests, and Arena demo.");
    puts("HTTPS app: web.app.https_listen(app, cert.pem, key.pem, port, host?, max_requests?).");
#ifdef HARIS_ANDROID
    puts("Android: use the NDK/CMake/JNI project; process sandbox workers are disabled because the app process is already OS-sandboxed.");
#endif
    puts("HTTP/3: build with -DHARIS_USE_HTTP3 and OpenSSL QUIC + nghttp3. WebRTC: build with -DHARIS_USE_WEBRTC + libdatachannel.");
}

static char *trim(char*s){while(isspace((unsigned char)*s))s++;char*e=s+strlen(s);while(e>s&&isspace((unsigned char)e[-1]))--e;*e=0;return s;}
static int write_text(const char*path,const char*text){FILE*f=fopen(path,"wb");if(!f)return 0;fputs(text,f);fclose(f);return 1;}
static int cmd_init(const char*name){const char*n=(name&&*name)?name:"HarisProject";char toml[2048];snprintf(toml,sizeof toml,"name=\"%s\"\nversion=\"0.0.1\"\nregistry=\"https://registry.haris.dev/index.tsv\"\n\n[dependencies]\n",n);if(!write_text("haris.toml",toml))return 1;remove("haris.lock");char path[512];snprintf(path,sizeof path,"%s.hr",n);if(!write_text(path,"print(\"Hello from Haris\")\n"))return 1;puts("Initialized Haris project");return 0;}

typedef struct {char name[128];char constraint[64];} Dep;
static int parse_toml_deps(Dep*deps,int max){FILE*f=fopen("haris.toml","rb");if(!f)return 0;char line[1024];int in=0,n=0;while(fgets(line,sizeof line,f)){char*t=trim(line);if(!strcmp(t,"[dependencies]")){in=1;continue;}if(in&&t[0]=='[')break;if(in&&*t&&t[0]!='#'){char*k=strtok(t,"=");char*v=strtok(NULL,"=");if(k&&v&&n<max){k=trim(k);v=trim(v);if(*v=='\"'){v++;char*e=strrchr(v,'\"');if(e)*e=0;}int seen=0;for(int j=0;j<n;j++)if(!strcmp(deps[j].name,k)){snprintf(deps[j].constraint,sizeof deps[j].constraint,"%s",v);seen=1;break;}if(!seen){snprintf(deps[n].name,sizeof deps[n].name,"%s",k);snprintf(deps[n].constraint,sizeof deps[n].constraint,"%s",v);n++;}}}}fclose(f);return n;}
static int semver_parts(const char*s,int p[3]){if(!s||sscanf(s,"%d.%d.%d",&p[0],&p[1],&p[2])<1)return 0;return 1;}
static int semver_cmp(const char*a,const char*b){int A[3]={0},B[3]={0};semver_parts(a,A);semver_parts(b,B);for(int i=0;i<3;i++)if(A[i]!=B[i])return A[i]>B[i]?1:-1;return 0;}
static int semver_match(const char*ver,const char*con){if(!con||!*con||!strcmp(con,"latest")||!strcmp(con,"*"))return 1;if(con[0]=='^'){int c[3]={0},v[3]={0};semver_parts(con+1,c);semver_parts(ver,v);return v[0]==c[0]&&semver_cmp(ver,con+1)>=0;}if(con[0]=='~'){int c[3]={0},v[3]={0};semver_parts(con+1,c);semver_parts(ver,v);return v[0]==c[0]&&v[1]==c[1]&&semver_cmp(ver,con+1)>=0;}if(!strncmp(con,">=",2))return semver_cmp(ver,con+2)>=0;if(!strncmp(con,"<=",2))return semver_cmp(ver,con+2)<=0;if(con[0]=='>')return semver_cmp(ver,con+1)>0;if(con[0]=='<')return semver_cmp(ver,con+1)<0;if(con[0]=='=')return semver_cmp(ver,con+1)==0;return semver_cmp(ver,con)==0;}
typedef struct {char*d;size_t n,cap;} HttpBuf;
static size_t http_buf_cb(void*p,size_t sz,size_t nm,void*u){HttpBuf*b=(HttpBuf*)u;size_t z=sz*nm;if(z>8ULL*1024ULL*1024ULL-b->n)return 0;if(b->n+z+1>b->cap){b->cap=(b->n+z+1)*2;b->d=(char*)xrealloc(b->d,b->cap);}memcpy(b->d+b->n,p,z);b->n+=z;b->d[b->n]=0;return z;}
static char* http_text(const char*url,size_t*len){if(!url||!len)return NULL;if(access(url,R_OK)==0){char*b=readf_limit(url,8ULL*1024ULL*1024ULL);if(!b)return NULL;*len=strlen(b);return b;}CURL*c=curl_easy_init();if(!c)return NULL;HttpBuf b={0};b.cap=4096;b.d=(char*)xmalloc(b.cap);curl_fast_defaults(c);curl_easy_setopt(c,CURLOPT_URL,url);curl_easy_setopt(c,CURLOPT_NOPROXY,"127.0.0.1,localhost");curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,http_buf_cb);curl_easy_setopt(c,CURLOPT_WRITEDATA,&b);CURLcode rc=curl_easy_perform(c);long code=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&code);curl_easy_cleanup(c);if(rc!=CURLE_OK||code<200||code>=300){xfree(b.d);return NULL;}*len=b.n;return b.d;}
static char g_registry_source[512]={0};
static const char* manifest_registry(void){static char reg[512];FILE*f=fopen("haris.toml","rb");if(!f)return NULL;char line[1024];while(fgets(line,sizeof line,f)){char*t=trim(line);if(!strncmp(t,"registry=",9)){char*v=trim(t+9);if(*v=='"'){v++;char*e=strrchr(v,'"');if(e)*e=0;}snprintf(reg,sizeof reg,"%s",v);fclose(f);return reg;}}fclose(f);return NULL;}
static int package_get(const char*name,const char*constraint,char*outver,size_t vos,char*outurl,size_t uos,char*outsha,size_t sos,char*outdeps,size_t dos){size_t len=0;const char*base=getenv("HARIS_REGISTRY");if(!base||!*base)base=manifest_registry();if(!base||!*base)base="https://registry.haris.dev/index.tsv";char*idx=http_text(base,&len);if(!idx)return 2;snprintf(g_registry_source,sizeof g_registry_source,"%s",base);char*save=0;for(char*line=strtok_r(idx,"\n",&save);line;line=strtok_r(NULL,"\n",&save)){char*n=strtok(line,"\t"),*v=strtok(NULL,"\t"),*u=strtok(NULL,"\t"),*h=strtok(NULL,"\t"),*d=strtok(NULL,"\t");if(n&&v&&u&&h&&!strcmp(n,name)&&semver_match(v,constraint)){if(!*outver||semver_cmp(v,outver)>0){snprintf(outver,vos,"%s",v);snprintf(outurl,uos,"%s",u);snprintf(outsha,sos,"%s",h);snprintf(outdeps,dos,"%s",d?d:"");}}}xfree(idx);return *outver?0:3;}
static int mkdir_p(const char*path){char tmp[1024];snprintf(tmp,sizeof tmp,"%s",path);for(char*p=tmp+1;*p;p++)if(*p=='/'){*p=0;mkdir(tmp,0755);*p='/';}return mkdir(tmp,0755)==0||errno==EEXIST;}

static int forge_name_ok(const char*n){if(!n||!*n)return 0;for(const char*p=n;*p;p++)if(!(isalnum((unsigned char)*p)||*p=='-'||*p=='_'))return 0;return 1;}
static int cmd_game_init(const char*name){
    const char*n=(name&&*name)?name:"HarisGame";
    if(!forge_name_ok(n)){fprintf(stderr,"haris game init: invalid project name\n");return 2;}
    char root[1024];snprintf(root,sizeof root,"%s",n);
    char srcdir[1024],assets[1024];snprintf(srcdir,sizeof srcdir,"%s/src",root);snprintf(assets,sizeof assets,"%s/assets",root);
    if(!mkdir_p(srcdir)||!mkdir_p(assets))return 1;
    char toml[2048],mainf[2048],gamef[2048],ignoref[2048],keepf[2048];
    snprintf(toml,sizeof toml,"name=\"%s\"\nversion=\"0.1.0\"\nengine=\"haris-3.6.0-forge\"\nmain=\"main.hr\"\n\n[game]\nfixed_dt=0.0166666667\nwidth=1280\nheight=720\n\n[dependencies]\n",n);
    snprintf(mainf,sizeof mainf,"%s/main.hr",root);
    const char*main_src =
        "world = engine.world()\n"
        "engine.gravity(world, 0, 9.8)\n"
        "player = game.spawn(world, 0, 0, 5, 0, 0.5, 100)\n"
        "enemy = game.spawn(world, 4, 0, -1, 0, 0.5, 50)\n"
        "game.fixed_step(world, 0.0166666667, 60)\n"
        "print(game.snapshot(world))\n"
        "print(\"Git:\")\n"
        "print(git.info(\".\"))\n";
    snprintf(gamef,sizeof gamef,"%s/src/game.hr",root);
    const char*game_src =
        "fn tick(world, dt) {\n"
        "    engine.step(world, dt)\n"
        "}\n";
    snprintf(ignoref,sizeof ignoref,"%s/.gitignore",root);
    const char*ignore =
        "haris.lock\n"
        "packages/\n"
        "build/\n"
        "*.log\n";
    snprintf(keepf,sizeof keepf,"%s/assets/.gitkeep",root);
    char pathbuf[1024];
    snprintf(pathbuf,sizeof pathbuf,"%s/haris.toml",root); if(!write_text(pathbuf,toml))return 1;
    if(!write_text(mainf,main_src)||!write_text(gamef,game_src)||!write_text(ignoref,ignore))return 1;
    if(!write_text(keepf,""))return 1;
    printf("Initialized Haris Forge game project: %s\n",root);return 0;
}
static int package_download(const char*url,const char*file){if(access(url,R_OK)==0){char*src=readf_limit(url,8ULL*1024ULL*1024ULL);if(!src)return 2;int ok=write_text(file,src);xfree(src);return ok?0:2;}CURL*c=curl_easy_init();if(!c)return 2;FILE*f=fopen(file,"wb");if(!f){curl_easy_cleanup(c);return 2;}curl_easy_setopt(c,CURLOPT_URL,url);curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"http,https");curl_easy_setopt(c,CURLOPT_NOPROXY,"127.0.0.1,localhost");curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(c,CURLOPT_TIMEOUT,30L);curl_easy_setopt(c,CURLOPT_MAXFILESIZE_LARGE,(curl_off_t)(8ULL*1024ULL*1024ULL));curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,fwrite);curl_easy_setopt(c,CURLOPT_WRITEDATA,f);CURLcode rc=curl_easy_perform(c);long code=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&code);fclose(f);curl_easy_cleanup(c);return (rc==CURLE_OK&&code>=200&&code<300)?0:2;}
static int valid_package_name(const char*n){if(!n||!*n)return 0;for(const char*p=n;*p;p++)if(!(isalnum((unsigned char)*p)||*p=='-'||*p=='_'))return 0;return 1;}
static int safe_package_component(const char*s){
    if(!s||!*s||!strcmp(s,".")||!strcmp(s,".."))return 0;
    size_t n=strlen(s);if(n>63)return 0;
    for(const unsigned char*p=(const unsigned char*)s;*p;p++) if(*p<33||*p=='/'||*p=='\\'||*p==':') return 0;
    return 1;
}
static char* registry_base_url(void){
    const char*base=getenv("HARIS_REGISTRY");
    if(!base||!*base)base="https://registry.haris.dev/index.tsv";
    static char out[512]; snprintf(out,sizeof out,"%s",base);
    size_t n=strlen(out); if(n>=9&&!strcmp(out+n-9,"index.tsv"))out[n-9]=0;
    while(n&&out[n-1]=='/')out[--n]=0; return out;
}
static int registry_hmac_hex(const unsigned char*data,size_t n,const char*key,char out[65]){
    if(!key||!*key){out[0]=0;return 1;} unsigned char mac[EVP_MAX_MD_SIZE]; unsigned int ml=0;
    if(!HMAC(EVP_sha256(),key,(int)strlen(key),data,n,mac,&ml)||ml!=32)return 0;
    for(unsigned int i=0;i<ml;i++)sprintf(out+i*2,"%02x",mac[i]); out[64]=0; return 1;
}
static char* b64_encode_bytes(const unsigned char*in,size_t n){
    static const char t[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t cap=((n+2)/3)*4+1; char*out=xmalloc(cap); size_t o=0;
    for(size_t i=0;i<n;i+=3){unsigned int v=(unsigned int)in[i]<<16;if(i+1<n)v|=(unsigned int)in[i+1]<<8;if(i+2<n)v|=in[i+2];out[o++]=t[(v>>18)&63];out[o++]=t[(v>>12)&63];out[o++]=(i+1<n)?t[(v>>6)&63]:'=';out[o++]=(i+2<n)?t[v&63]:'=';} out[o]=0; return out;
}
static int registry_publish_cli(const char*name,const char*ver,const char*file,const char*desc){
    if(!valid_package_name(name)||!ver||!*ver||!file){fprintf(stderr,"haris publish: invalid package metadata\n");return 2;}
    int vp[3]; if(!semver_parts(ver,vp)){fprintf(stderr,"haris publish: version must look like X.Y.Z\n");return 2;}
    FILE*f=fopen(file,"rb"); if(!f)return 2; if(fseek(f,0,SEEK_END)!=0){fclose(f);return 2;} long sz=ftell(f); if(sz<0||sz>(long)H_DATA_MAX){fclose(f);return 2;} if(fseek(f,0,SEEK_SET)!=0){fclose(f);return 2;}
    size_t n=(size_t)sz; unsigned char*raw=xmalloc(n?n:1); size_t got=fread(raw,1,n,f); fclose(f); if(got!=n){xfree(raw);return 2;}
    char sha[65],sig[65]={0}; if(!sha256_file(file,sha)){xfree(raw);return 2;} if(!registry_hmac_hex(raw,n,getenv("HARIS_REGISTRY_KEY"),sig)){xfree(raw);return 2;}
    char*enc=b64_encode_bytes(raw,n); xfree(raw);
    HJsonBuf b={0}; if(!hjson_puts(&b,"{\"name\":" )||!hjson_string(&b,name)||!hjson_puts(&b,",\"version\":")||!hjson_string(&b,ver)||!hjson_puts(&b,",\"description\":")||!hjson_string(&b,desc?desc:"")||!hjson_puts(&b,",\"deps\":\"\",\"content_b64\":")||!hjson_string(&b,enc)||!hjson_puts(&b,",\"sha256\":")||!hjson_string(&b,sha)||!hjson_puts(&b,",\"signature\":")||!hjson_string(&b,getenv("HARIS_REGISTRY_KEY")?sig:"")||!hjson_putc(&b,'}')){xfree(enc);xfree(b.d);return 2;} xfree(enc);
    char url[640];snprintf(url,sizeof url,"%s/publish",registry_base_url()); CURL*c=curl_easy_init();if(!c){xfree(b.d);return 2;} struct curl_slist*h=curl_slist_append(NULL,"Content-Type: application/json");FILE*out=tmpfile();if(!out){curl_slist_free_all(h);curl_easy_cleanup(c);xfree(b.d);return 2;} CurlOut ob={out,0};
    curl_easy_setopt(c,CURLOPT_URL,url);curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"http,https");curl_easy_setopt(c,CURLOPT_REDIR_PROTOCOLS_STR,"http,https");curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(c,CURLOPT_TIMEOUT,60L);curl_easy_setopt(c,CURLOPT_POST,1L);curl_easy_setopt(c,CURLOPT_POSTFIELDS,b.d);curl_easy_setopt(c,CURLOPT_HTTPHEADER,h);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,cw);curl_easy_setopt(c,CURLOPT_WRITEDATA,&ob);
    CURLcode rc=curl_easy_perform(c);long code=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&code);curl_slist_free_all(h);curl_easy_cleanup(c);xfree(b.d);fflush(out);
    if(rc!=CURLE_OK){fprintf(stderr,"haris publish: %s\n",curl_easy_strerror(rc));fclose(out);return 2;} if(fseek(out,0,SEEK_END)!=0){fclose(out);return 2;} long olen=ftell(out);if(olen<0||olen>(long)H_DATA_MAX){fclose(out);return 2;}if(fseek(out,0,SEEK_SET)!=0){fclose(out);return 2;}char*resp=xmalloc((size_t)olen+1);size_t rg=fread(resp,1,(size_t)olen,out);resp[rg]=0;fclose(out);printf("HTTP %ld\n%s\n",code,resp);xfree(resp);return (code>=200&&code<300)?0:3;
}
static int registry_stats_cli(void){char url[640];snprintf(url,sizeof url,"%s/stats.json",registry_base_url());size_t len=0;char*b=http_text(url,&len);if(!b){fprintf(stderr,"haris: registry unavailable\n");return 2;}printf("%s\n",b);xfree(b);return 0;}
static int package_source_from_registry(const char*url,char*out,size_t cap){if(access(url,R_OK)==0){snprintf(out,cap,"%s",url);return 1;}const char*slash=strrchr(g_registry_source,'/');if(slash&&strncmp(g_registry_source,"http://",7)&&strncmp(g_registry_source,"https://",8)){size_t L=(size_t)(slash-g_registry_source+1);if(L+strlen(url)+1<cap){memcpy(out,g_registry_source,L);snprintf(out+L,cap-L,"%s",url);if(access(out,R_OK)==0)return 1;}}snprintf(out,cap,"%s",url);return 0;}
static int lock_append(const char*name,const char*ver,const char*url,const char*sha){FILE*f=fopen("haris.lock","rb");if(f){char line[1024],n0[128],v0[64];while(fgets(line,sizeof line,f)){if(sscanf(line,"%127[^\t]\t%63[^\t]",n0,v0)==2&&!strcmp(n0,name)&&!strcmp(v0,ver)){fclose(f);return 1;}}fclose(f);}f=fopen("haris.lock","ab");if(!f)return 0;fprintf(f,"%s\t%s\t%s\t%s\n",name,ver,url,sha);fclose(f);return 1;}
static int sha256_file(const char*path,char out[65]){FILE*f=fopen(path,"rb");if(!f)return 0;EVP_MD_CTX*c=EVP_MD_CTX_new();if(!c){fclose(f);return 0;}if(EVP_DigestInit_ex(c,EVP_sha256(),NULL)!=1){EVP_MD_CTX_free(c);fclose(f);return 0;}unsigned char buf[8192];size_t n;while((n=fread(buf,1,sizeof buf,f))>0)if(EVP_DigestUpdate(c,buf,n)!=1){EVP_MD_CTX_free(c);fclose(f);return 0;}fclose(f);unsigned char h[EVP_MAX_MD_SIZE];unsigned int hn=0;int ok=EVP_DigestFinal_ex(c,h,&hn)==1;EVP_MD_CTX_free(c);if(!ok||hn!=32)return 0;for(unsigned int i=0;i<hn;i++)sprintf(out+i*2,"%02x",h[i]);out[64]=0;return 1;}
static int package_install_one(const char*name,const char*constraint,int depth){if(depth>32)return 2;if(!valid_package_name(name))return 2;char ver[64]={0},url[512]={0},sha[65]={0},deps[1024]={0};int rc=package_get(name,constraint,ver,sizeof ver,url,sizeof url,sha,sizeof sha,deps,sizeof deps);if(rc)return rc;if(!safe_package_component(ver)){fprintf(stderr,"haris: unsafe package version from registry\n");return 6;}char dir[512],file[768],src[768];snprintf(dir,sizeof dir,"packages/%s/%s",name,ver);if(!mkdir_p(dir))return 2;snprintf(file,sizeof file,"%s/main.hr",dir);if(package_source_from_registry(url,src,sizeof src))rc=package_download(src,file);else rc=package_download(url,file);if(rc)return rc;char got[65];if(!sha256_file(file,got)||strcasecmp(got,sha)){fprintf(stderr,"haris: SHA-256 mismatch for %s@%s\n",name,ver);remove(file);return 4;}char current[512];snprintf(current,sizeof current,"packages/%s/__init__.hr",name);char*txt=readf_limit(file,8ULL*1024ULL*1024ULL);if(!txt)return 2;if(!write_text(current,txt)){xfree(txt);return 2;}xfree(txt);if(deps[0]){char depcopy[1024];snprintf(depcopy,sizeof depcopy,"%s",deps);char*save=0;for(char*d=strtok_r(depcopy,";",&save);d;d=strtok_r(NULL,";",&save)){char*at=strchr(d,'@');char dn[128],dc[64]="*";if(at){size_t L=(size_t)(at-d);if(L>=sizeof dn)L=sizeof dn-1;memcpy(dn,d,L);dn[L]=0;snprintf(dc,sizeof dc,"%s",at+1);}else snprintf(dn,sizeof dn,"%s",d);if(package_install_one(trim(dn),trim(dc),depth+1))return 5;}}if(!lock_append(name,ver,url,sha))return 2;printf("Installed %s@%s\n",name,ver);return 0;}
static int remove_dep_from_manifest(const char*name){FILE*f=fopen("haris.toml","rb");if(!f)return 2;FILE*o=fopen("haris.toml.tmp","wb");if(!o){fclose(f);return 2;}char line[1024];int in=0;char key[160];snprintf(key,sizeof key,"%s=",name);while(fgets(line,sizeof line,f)){char*t=trim(line);if(!strcmp(t,"[dependencies]"))in=1;if(in&&t[0]=='['&&strcmp(t,"[dependencies]"))in=0;if(!(in&&!strncmp(t,key,strlen(key))))fputs(line,o);}fclose(f);fclose(o);if(rename("haris.toml.tmp","haris.toml")!=0){remove("haris.toml.tmp");return 2;}return 0;}
static int manifest_has_dep(const char*name){Dep deps[128];int n=parse_toml_deps(deps,128);for(int i=0;i<n;i++)if(!strcmp(deps[i].name,name))return 1;return 0;}
static int remove_tree(const char *path){
    if(!path||!*path)return 0;
#ifdef _WIN32
    struct _stat st;
    if(_stat(path,&st)!=0)return errno==ENOENT;
    DWORD attrs=GetFileAttributesA(path);
    if(attrs!=INVALID_FILE_ATTRIBUTES && (attrs&FILE_ATTRIBUTE_REPARSE_POINT)) return DeleteFileA(path)!=0 || GetLastError()==ERROR_FILE_NOT_FOUND;
    if(!(st.st_mode&_S_IFDIR))return DeleteFileA(path)!=0 || errno==ENOENT;
    struct _finddata_t fd;
    char pat[PATH_MAX];
    int w=snprintf(pat,sizeof pat,"%s\\*",path);
    if(w<0||(size_t)w>=sizeof pat)return 0;
    intptr_t h=_findfirst(pat,&fd);
    if(h!=-1){
        do{
            if(!strcmp(fd.name,".")||!strcmp(fd.name,".."))continue;
            char child[PATH_MAX];
            w=snprintf(child,sizeof child,"%s\\%s",path,fd.name);
            if(w<0||(size_t)w>=sizeof child){_findclose(h);return 0;}
            if(!remove_tree(child)){_findclose(h);return 0;}
        }while(_findnext(h,&fd)==0);
        _findclose(h);
    }
    return RemoveDirectoryA(path)!=0;
#else
    struct stat st;
    if(lstat(path,&st)!=0)return errno==ENOENT;
    if(!S_ISDIR(st.st_mode))return unlink(path)==0 || errno==ENOENT;
    DIR *d=opendir(path);
    if(!d)return 0;
    struct dirent *ent;
    int ok=1;
    while((ent=readdir(d))){
        if(!strcmp(ent->d_name,".")||!strcmp(ent->d_name,".."))continue;
        char child[PATH_MAX];
        int w=snprintf(child,sizeof child,"%s/%s",path,ent->d_name);
        if(w<0||(size_t)w>=sizeof child){ok=0;break;}
        if(!remove_tree(child)){ok=0;break;}
    }
    closedir(d);
    if(!ok)return 0;
    return rmdir(path)==0 || errno==ENOENT;
#endif
}

static int package_edit(const char*spec,int remove_mode){
 char name[128]={0},con[64]="*";const char*at=strchr(spec,'@');
 if(at){size_t L=(size_t)(at-spec);if(L>=sizeof name)L=sizeof name-1;memcpy(name,spec,L);name[L]=0;snprintf(con,sizeof con,"%s",at+1);}else snprintf(name,sizeof name,"%s",spec);
 if(!*name||!valid_package_name(name))return 2;
 if(remove_mode){
  char dir[512];int w=snprintf(dir,sizeof dir,"packages/%s",name);if(w<0||(size_t)w>=sizeof dir)return 2;
  if(!remove_tree(dir) && errno!=ENOENT){fprintf(stderr,"haris: failed to remove package directory '%s'\n",dir);return 2;}
  if(remove_dep_from_manifest(name)!=0){fprintf(stderr,"haris: warning: failed to update haris.toml\n");return 2;}
  remove("haris.lock");printf("Removed %s; run haris update to regenerate lockfile\n",name);return 0;
 }
 int rc=package_install_one(name,con,0);if(rc)return rc;FILE*f=fopen("haris.toml","rb");if(!f)return 2;char buf[16384];size_t n=fread(buf,1,sizeof(buf)-1,f);fclose(f);buf[n]=0;FILE*o=fopen("haris.toml.tmp","wb");if(!o)return 2;char*save=0;int inserted=0;for(char*line=strtok_r(buf,"\n",&save);line;line=strtok_r(NULL,"\n",&save)){fputs(line,o);fputc('\n',o);if(!strcmp(trim(line),"[dependencies]")&&!manifest_has_dep(name)){fprintf(o,"%s=\"%s\"\n",name,con);inserted=1;}}if(!inserted&&!manifest_has_dep(name))fprintf(o,"\n[dependencies]\n%s=\"%s\"\n",name,con);fclose(o);if(rename("haris.toml.tmp","haris.toml")!=0){remove("haris.toml.tmp");return 2;}return 0;}
static int package_update(void){Dep deps[128];int n=parse_toml_deps(deps,128);remove("haris.lock");for(int i=0;i<n;i++){int rc=package_install_one(deps[i].name,deps[i].constraint,0);if(rc)return rc;}puts("Updated dependencies and regenerated haris.lock");return 0;}
static int package_lock(void){return package_update();}

/* Run a test file without invoking a shell. This removes command injection
   through paths and also works when a test path contains spaces or shell
   metacharacters. */
static int run_file_cmd(const char*path){
    if(!path||!*path)return 2;
#ifdef HARIS_ANDROID
    (void)path; return 2;
#elif defined(_WIN32)
    char *const av[]={(char*)g_program_path,(char*)path,NULL};
    intptr_t rc=_spawnv(_P_WAIT,g_program_path,(const char *const*)av);
    if(rc==-1)return 127;
    return (int)rc;
#else
    pid_t pid=fork();
    if(pid<0)return 127;
    if(pid==0){
        char *const av[]={(char*)g_program_path,(char*)path,NULL};
        execv(g_program_path,av);
        _exit(127);
    }
    int st=0;
    if(waitpid(pid,&st,0)<0)return 127;
    if(WIFEXITED(st))return WEXITSTATUS(st);
    if(WIFSIGNALED(st))return 128+WTERMSIG(st);
    return 127;
#endif
}

static int is_test_file_name(const char *name){
    if(!name||!*name)return 0;
    size_t n=strlen(name), s=strlen(".test.hr");
    if(n>=s && !strcmp(name+n-s,".test.hr"))return 1;
    return n>=8 && !strncmp(name,"test_",5) && n>=3 && !strcmp(name+n-3,".hr");
}

#ifndef _WIN32
static int discover_and_run_tests(const char *root,int *total,int *passed){
    struct stat st;
    if(lstat(root,&st)!=0){fprintf(stderr,"haris test: cannot access '%s': %s\n",root,strerror(errno));return 2;}
    if(!S_ISDIR(st.st_mode)){
        if(is_test_file_name(root)){
            (*total)++;
            int rc=run_file_cmd(root);
            if(rc==0){(*passed)++;printf("PASS %s\n",root);}else printf("FAIL %s\n",root);
            return 0;
        }
        fprintf(stderr,"haris test: '%s' is not a test directory\n",root);return 2;
    }
    DIR *d=opendir(root);
    if(!d){fprintf(stderr,"haris test: cannot open '%s': %s\n",root,strerror(errno));return 2;}
    struct dirent *ent; int rc_all=0;
    while((ent=readdir(d))){
        if(!strcmp(ent->d_name,".")||!strcmp(ent->d_name,".."))continue;
        char child[PATH_MAX];int w=snprintf(child,sizeof child,"%s/%s",root,ent->d_name);
        if(w<0||(size_t)w>=sizeof child){fprintf(stderr,"haris test: path too long: %s/%s\n",root,ent->d_name);rc_all=2;continue;}
        struct stat cst;
        if(lstat(child,&cst)!=0){fprintf(stderr,"haris test: cannot stat '%s': %s\n",child,strerror(errno));rc_all=2;continue;}
        if(S_ISDIR(cst.st_mode)){
            /* Never follow symlinked directories: lstat() above makes this explicit. */
            if(discover_and_run_tests(child,total,passed)!=0)rc_all=2;
        }else if(S_ISREG(cst.st_mode)&&is_test_file_name(ent->d_name)){
            (*total)++;
            int rc=run_file_cmd(child);
            if(rc==0){(*passed)++;printf("PASS %s\n",child);}else printf("FAIL %s\n",child);
        }
    }
    closedir(d);return rc_all;
}
#else
static int discover_and_run_tests(const char *root,int *total,int *passed){
    DWORD attr=GetFileAttributesA(root);
    if(attr==INVALID_FILE_ATTRIBUTES){fprintf(stderr,"haris test: cannot access '%s'\n",root);return 2;}
    if(!(attr&FILE_ATTRIBUTE_DIRECTORY)){
        if(is_test_file_name(root)){
            (*total)++;int rc=run_file_cmd(root);
            if(rc==0){(*passed)++;printf("PASS %s\n",root);}else printf("FAIL %s\n",root);
            return 0;
        }
        fprintf(stderr,"haris test: '%s' is not a test directory\n",root);return 2;
    }
    char pat[PATH_MAX];int w=snprintf(pat,sizeof pat,"%s\\*",root);if(w<0||(size_t)w>=sizeof pat)return 2;
    struct _finddata_t fd;intptr_t h=_findfirst(pat,&fd);if(h==-1)return 0;
    int rc_all=0;
    do{
        if(!strcmp(fd.name,".")||!strcmp(fd.name,".."))continue;
        char child[PATH_MAX];w=snprintf(child,sizeof child,"%s\\%s",root,fd.name);if(w<0||(size_t)w>=sizeof child){rc_all=2;continue;}
        if(fd.attrib&_A_SUBDIR){if(discover_and_run_tests(child,total,passed)!=0)rc_all=2;}
        else if(is_test_file_name(fd.name)){(*total)++;int rc=run_file_cmd(child);if(rc==0){(*passed)++;printf("PASS %s\n",child);}else printf("FAIL %s\n",child);}
    }while(_findnext(h,&fd)==0);
    _findclose(h);return rc_all;
}
#endif

static int builtin_test_web_batch_result(void){
    VM vm;init(&vm);Value urls=va();ap(urls.u.a,vs("bad://haris.invalid"));Value args[2]={urls,vi(1)};Value out=web_batch(&vm,2,args);
    if(out.t!=VARR||out.u.a->n!=1)return 0;Value r=out.u.a->v[0],ok=stget(r.u.st,"ok"),err=stget(r.u.st,"error");
    return ok.t==VBOOL&&!ok.u.b&&err.t==VSTR;
}
static int builtin_test_gfx_text(void){VM vm;init(&vm);Value a[5]={vi(0),vi(0),vs("Haris 123"),vs("#ffffff"),vi(1)};Value r=gfx_draw_text(&vm,5,a);return r.t==VBOOL&&r.u.b;}
static int builtin_test_git_parser(void){
    const char*s="# branch.oid deadbeef\n# branch.head main\n# branch.upstream origin/main\n# branch.ab +3 -2\n1 .M N... 100644 100644 100644 abc def file.txt\n";
    char br[64],hd[64],up[128];long long a=0,b=0;int d=0,det=0;
    if(!git_info_parse_status(s,br,sizeof br,hd,sizeof hd,&a,&b,&d,&det,up,sizeof up))return 0;
    return !strcmp(br,"main")&&!strcmp(hd,"deadbeef")&&!strcmp(up,"origin/main")&&a==3&&b==-2&&d&&!det;
}
static int builtin_test_data_smoke(void){
    VM vm;init(&vm);Value rows=va(),r1=va(),r2=va();
    ap(r1.u.a,vi(1));ap(r1.u.a,vi(2));ap(r2.u.a,vi(3));ap(r2.u.a,vi(4));ap(rows.u.a,r1);ap(rows.u.a,r2);
    Value cols=va();ap(cols.u.a,vs("a"));ap(cols.u.a,vs("b"));Value args[2]={rows,cols};Value df=data_from_rows(&vm,2,args);
    if(!df_is(df))return 0;Value sh[1]={df};Value shape=data_shape(&vm,1,sh);
    return shape.t==VARR&&shape.u.a->n==2&&shape.u.a->v[0].t==VINT&&shape.u.a->v[0].u.i==2&&shape.u.a->v[1].t==VINT&&shape.u.a->v[1].u.i==2;
}
static int builtin_test_memory_safe(void){
    VM vm;init(&vm);Value m=memory_alloc(&vm,1,(Value[]){vi(32)});if(m.t!=VHANDLE)return 0;
    if(!memory_write_u8(&vm,3,(Value[]){m,vi(7),vi(173)}).u.b)return 0;
    Value r=memory_read_u8(&vm,2,(Value[]){m,vi(7)});if(r.t!=VINT||r.u.i!=173)return 0;
    if(memory_read_u8(&vm,2,(Value[]){m,vi(32)}).t!=VNULL)return 0;
    if(!memory_resize(&vm,2,(Value[]){m,vi(64)}).u.b)return 0;
    if(memory_size(&vm,1,(Value[]){m}).u.i!=64)return 0;
    if(!memory_write_u16(&vm,3,(Value[]){m,vi(8),vi(4660)}).u.b)return 0;
    if(memory_read_u16(&vm,2,(Value[]){m,vi(8)}).u.i!=4660)return 0;
    if(!memory_write_f32(&vm,3,(Value[]){m,vi(16),vf(3.5)}).u.b)return 0;
    if(fabs(dn(memory_read_f32(&vm,2,(Value[]){m,vi(16)}))-3.5)>1e-6)return 0;
    return memory_free(&vm,1,(Value[]){m}).u.b&&memory_read_u8(&vm,2,(Value[]){m,vi(7)}).t==VNULL;
}
static int builtin_test_memory_raw_gate(void){
    VM vm;init(&vm);unsigned char buf[8]={0};Value wrapargs[2]={vi((long long)(uintptr_t)buf),vi(8)};
    vm.module_sandboxed=1;
    if(memory_wrap(&vm,2,wrapargs).t!=VNULL)return 0;
    vm.module_caps=CAP_NUCLEAR;Value m=memory_wrap(&vm,2,wrapargs);if(m.t!=VHANDLE)return 0;
    if(!memory_write_u8(&vm,3,(Value[]){m,vi(3),vi(99)}).u.b)return 0;
    return buf[3]==99;
}
static int builtin_test_handle_safety(void){
    VM vm;init(&vm);Value m=memory_alloc(&vm,1,(Value[]){vi(4)});if(m.t!=VHANDLE)return 0;memory_free(&vm,1,(Value[]){m});
    return !memory_write_u8(&vm,3,(Value[]){m,vi(0),vi(1)}).u.b && memory_size(&vm,1,(Value[]){m}).t==VNULL;
}
static int builtin_test_math_edges(void){
    VM vm;init(&vm);Value r=nabs(&vm,1,(Value[]){vi(LLONG_MIN)});if(r.t!=VNULL)return 0;
    Value q=nrange(&vm,3,(Value[]){vi(10),vi(-10),vi(-3)});return q.t==VARR&&q.u.a->n==7&&q.u.a->v[0].u.i==10&&q.u.a->v[6].u.i==-8;
}
static int builtin_test_mlp_fast_path(void){
    VM vm;init(&vm);Value m=ai_mlp(&vm,3,(Value[]){vi(3),vi(8),vi(4)});if(m.t!=VHANDLE)return 0;HMLP*mm=(HMLP*)m.u.handle;double l1=ai_xavier_limit(3,8),l2=ai_xavier_limit(8,4);for(int i=0;i<3*8;i++)if(!isfinite(mm->w1[i])||fabs(mm->w1[i])>l1)return 0;for(int i=0;i<8*4;i++)if(!isfinite(mm->w2[i])||fabs(mm->w2[i])>l2)return 0;
    Value x=va();ap(x.u.a,vf(0.1));ap(x.u.a,vf(-0.2));ap(x.u.a,vf(0.7));Value p=ai_mlp_predict(&vm,2,(Value[]){m,x});if(p.t!=VARR||p.u.a->n!=4)return 0;Value pr=ai_mlp_predict_proba(&vm,2,(Value[]){m,x});double sum=0;for(size_t i=0;i<pr.u.a->n;i++)sum+=dn(pr.u.a->v[i]);
    if(!isfinite(sum)||fabs(sum-1.0)>1e-9)return 0;
    Value too=va();too.u.a->n=HARIS_AI_BATCH_MAX+1;
    Value br=ai_mlp_predict_batch(&vm,2,(Value[]){m,too});
    if(br.t!=VNULL)return 0;
    if(nmath_clear_overflow(&vm,0,NULL).t!=VBOOL)return 0;vm.module_sandboxed=1;vm.int_overflow=0;long long ov=int_binop(&vm,I_ADD,LLONG_MAX,1);Value flag=nmath_overflowed(&vm,0,NULL);vm.module_sandboxed=0;return ov==0&&flag.t==VBOOL&&flag.u.b;
}
static int builtin_test_mlp_train_class(void){
    VM vm;init(&vm);Value m=ai_mlp(&vm,3,(Value[]){vi(2),vi(6),vi(2)});if(m.t!=VHANDLE)return 0;Value xs=va(),ys=va();
    for(int i=0;i<6;i++){Value x=va();ap(x.u.a,vf((double)(i%2)));ap(x.u.a,vf((double)(i<3)));ap(xs.u.a,x);ap(ys.u.a,vi(i%2));}
    return ai_mlp_train_class(&vm,5,(Value[]){m,xs,ys,vf(0.03),vi(2)}).u.b;
}
static int builtin_test_deep_mlp(void){
    VM vm;init(&vm);Value sizes=va();ap(sizes.u.a,vi(3));ap(sizes.u.a,vi(5));ap(sizes.u.a,vi(4));ap(sizes.u.a,vi(2));Value m=ai_deep_mlp(&vm,1,&sizes);if(m.t!=VHANDLE)return 0;HDMLP*dm=(HDMLP*)m.u.handle;for(int l=0;l<dm->nlayers;l++){double lim=ai_xavier_limit(dm->sizes[l],dm->sizes[l+1]);size_t z=(size_t)dm->sizes[l]*(size_t)dm->sizes[l+1];for(size_t i=0;i<z;i++)if(!isfinite(dm->W[l][i])||fabs(dm->W[l][i])>lim)return 0;}Value x=va();ap(x.u.a,vf(0.1));ap(x.u.a,vf(0.2));ap(x.u.a,vf(0.3));Value p=ai_deep_mlp_predict(&vm,2,(Value[]){m,x});if(p.t!=VARR||p.u.a->n!=2)return 0;Value too=va();too.u.a->n=HARIS_AI_BATCH_MAX+1;Value br=ai_deep_mlp_predict_batch(&vm,2,(Value[]){m,too});return br.t==VNULL;
}
static int builtin_test_engine_scene(void){
    VM vm;init(&vm);Value s=engine_scene(&vm,0,NULL);Value a=engine_node(&vm,2,(Value[]){s,vs("root")});Value b=engine_node(&vm,3,(Value[]){s,vs("player"),vs("Sprite")});if(a.t!=VHANDLE||b.t!=VHANDLE)return 0;if(!engine_add_child(&vm,3,(Value[]){s,a,b}).u.b)return 0;Value i=engine_node_info(&vm,1,(Value[]){a});return i.t==VARR&&i.u.a->n==9&&i.u.a->v[8].u.i==1;
}
static int builtin_test_engine_physics(void){
    VM vm;init(&vm);Value w=engine_world(&vm,0,NULL);Value e=engine_entity(&vm,1,(Value[]){w});if(e.t!=VHANDLE)return 0;engine_set_velocity(&vm,3,(Value[]){e,vf(10),vf(0)});engine_step(&vm,2,(Value[]){w,vf(0.5)});Value p=engine_position(&vm,1,(Value[]){e});return p.t==VARR&&p.u.a->n==2&&fabs(dn(p.u.a->v[0])-5.0)<1e-9;
}
static int builtin_test_grc_surface(void){
    VM vm;init(&vm);
    Value c=compliance_status(&vm,0,NULL);
    Value a[1]={vs("smoke")};Value au=audit_event(&vm,1,a);
    Value args[1]={c};Value r=report_json(&vm,1,args);
    return c.t==VSTRUCT&&au.t==VSTRUCT&&r.t==VSTR&&strstr(r.u.s,"sandbox_default")!=NULL;
}

static int builtin_test_udp_loopback(void){VM vm;init(&vm);Value srv=udp_bind(&vm,2,(Value[]){vs("127.0.0.1"),vi(0)});if(srv.t!=VHANDLE)return 0;HUDP*u=udp_handle(srv);if(!u||u->s==HARIS_INVALID_SOCKET)return 0;struct sockaddr_storage ss;socklen_t sl=sizeof ss;if(getsockname(u->s,(struct sockaddr*)&ss,&sl)!=0)return 0;int port=ss.ss_family==AF_INET?ntohs(((struct sockaddr_in*)&ss)->sin_port):ntohs(((struct sockaddr_in6*)&ss)->sin6_port);Value sock=udp_open(&vm,3,(Value[]){vs("127.0.0.1"),vi(port),vb(1)});if(sock.t!=VHANDLE)return 0;Value payload=vs("ping");Value ok=udp_send(&vm,2,(Value[]){sock,payload});if(!ok.u.b)return 0;Value got=udp_recv(&vm,3,(Value[]){srv,vi(32),vi(1000)});return got.t==VSTRUCT&&stget(got.u.st,"size").t==VINT&&stget(got.u.st,"size").u.i==4;}
static int builtin_test_game_ai_server_bot(void){VM vm;init(&vm);Value s=game_server_create(&vm,3,(Value[]){vi(60),vi(8),vi(64)});if(s.t!=VHANDLE)return 0;Value b=ai_game_brain(&vm,1,(Value[]){vi(1800)});if(b.t!=VHANDLE)return 0;Value r=game_server_add_bot(&vm,2,(Value[]){s,b});if(r.t!=VSTRUCT)return 0;const char*id=stget(r.u.st,"player").u.s;for(int i=0;i<40;i++)game_server_tick(&vm,2,(Value[]){s,vi((i+1)*17)});Value bi=game_server_bot_info(&vm,2,(Value[]){s,vs(id)});if(bi.t!=VSTRUCT)return 0;return stget(bi.u.st,"replay").t==VINT&&stget(bi.u.st,"replay").u.i>0&&stget(bi.u.st,"learn_steps").t==VINT;}
static int builtin_test_game_spectator_replay(void){VM vm;init(&vm);Value s=game_server_create(&vm,2,(Value[]){vi(30),vi(8)});Value j=game_server_join(&vm,2,(Value[]){s,vs("khalid")});if(j.t!=VSTRUCT)return 0;for(int i=0;i<5;i++)game_server_tick(&vm,2,(Value[]){s,vi((i+1)*34)});Value sp=game_server_spectate(&vm,2,(Value[]){s,vs("khalid")});if(sp.t!=VSTRUCT||stget(sp.u.st,"spectating").t!=VSTR)return 0;char path[PATH_MAX];snprintf(path,sizeof path,"haris_test_replay_%ld.replay",(long)getpid());Value sv=game_server_replay_save(&vm,2,(Value[]){s,vs(path)});if(sv.t!=VBOOL||!sv.u.b)return 0;Value rp=game_server_replay_play(&vm,1,(Value[]){vs(path)});remove(path);return rp.t==VSTRUCT&&stget(rp.u.st,"frame_count").t==VINT&&stget(rp.u.st,"frame_count").u.i>0;}
static int builtin_test_game_ranked(void){VM vm;init(&vm);Value s=game_server_create(&vm,2,(Value[]){vi(30),vi(8)});if(game_server_join(&vm,2,(Value[]){s,vs("khalid")}).t!=VSTRUCT)return 0;if(game_server_join(&vm,2,(Value[]){s,vs("player2")}).t!=VSTRUCT)return 0;if(game_server_match_start(&vm,1,&s).t!=VINT)return 0;if(!game_server_match_end(&vm,2,(Value[]){s,vs("khalid")}).u.b)return 0;Value r=game_server_rank(&vm,2,(Value[]){s,vs("khalid")});Value lb=game_server_leaderboard(&vm,1,&s);return r.t==VSTRUCT&&stget(r.u.st,"games").u.i==1&&stget(r.u.st,"wins").u.i==1&&lb.t==VARR&&lb.u.a->n==2;}
static int builtin_test_game_admin_cmd(void){VM vm;init(&vm);Value s=game_server_create(&vm,2,(Value[]){vi(30),vi(8)});if(game_server_join(&vm,2,(Value[]){s,vs("player5")}).t!=VSTRUCT)return 0;Value k=game_server_cmd(&vm,2,(Value[]){s,vs("kick player5")});Value bad=game_server_cmd(&vm,2,(Value[]){s,vs("system uname")});return k.t==VSTRUCT&&stget(k.u.st,"ok").t==VBOOL&&stget(k.u.st,"ok").u.b&&bad.t==VSTRUCT&&!stget(bad.u.st,"ok").u.b;}

static int builtin_test_game_ai(void){VM vm;init(&vm);Value b=ai_game_brain(&vm,3,(Value[]){vi(4),vi(16),vi(3)});if(b.t!=VHANDLE)return 0;Value s=va(),ns=va();for(int i=0;i<4;i++){ap(s.u.a,vf(i==0));ap(ns.u.a,vf(i==1));}Value act=ai_game_act(&vm,2,(Value[]){b,s});if(act.t!=VINT)return 0;if(!ai_game_remember(&vm,6,(Value[]){b,s,act,vf(1.0),ns,vb(0)}).u.b)return 0;return ai_game_train_step(&vm,2,(Value[]){b,vi(4)}).t==VINT;}
static int builtin_test_nav_astar(void){VM vm;init(&vm);Value g=va();int w=5,h=5;for(int i=0;i<w*h;i++)ap(g.u.a,vi(0));ap(g.u.a,vi(1)); /* malformed extra obstacle trimmed below */ g.u.a->n--; g.u.a->v[1]=vi(1);Value p=game_nav_astar(&vm,7,(Value[]){g,vi(w),vi(h),vi(0),vi(0),vi(4),vi(0)});return p.t==VARR&&p.u.a->n>=5&&p.u.a->v[0].u.i==0&&p.u.a->v[p.u.a->n-1].u.i==4;}

static int builtin_test_websocket_frame(void){
#ifndef _WIN32
    int sv[2];if(socketpair(AF_UNIX,SOCK_STREAM,0,sv)!=0)return 0;HWS a={0},b={0};a.kind=HK_WS;a.s=sv[0];a.is_server=0;a.closed=0;b.kind=HK_WS;b.s=sv[1];b.is_server=1;b.closed=0;const char*msg="hello";if(!ws_send_frame(&a,1,(const unsigned char*)msg,5,0)){close(sv[0]);close(sv[1]);return 0;}int fin,op;unsigned char*p=NULL;size_t z=0;int ok=ws_read_frame(&b,&fin,&op,&p,&z);int pass=ok&&fin&&op==1&&z==5&&!memcmp(p,"hello",5);xfree(p);close(sv[0]);close(sv[1]);return pass;
#else
    return 1;
#endif
}

static int builtin_test_sandbox_roundtrip(void){
    VM vm;init(&vm);Value a[1]={vs("print(21*2)")};Value r=sandbox_run_code(&vm,1,a);if(r.t!=VSTRUCT)return 0;Value ok=stget(r.u.st,"ok"),out=stget(r.u.st,"output");return ok.t==VBOOL&&ok.u.b&&out.t==VSTR&&strstr(out.u.s,"42")!=NULL;
}
static int builtin_test_tensor_autograd(void){
    VM vm;init(&vm);Value d=va();ap(d.u.a,vf(2));ap(d.u.a,vf(3));Value sh=va();ap(sh.u.a,vi(2));Value x=tensor_from_array(&vm,3,(Value[]){d,sh,vb(1)});if(!tensor_handle(x))return 0;Value y=tensor_mul(&vm,2,(Value[]){x,x});Value loss=tensor_sum(&vm,1,&y);if(!tensor_handle(loss)||!tensor_backward(&vm,1,&loss).u.b)return 0;Value g=tensor_grad(&vm,1,&x);return g.t==VARR&&g.u.a->n==2&&fabs(dn(g.u.a->v[0])-4.0)<1e-9&&fabs(dn(g.u.a->v[1])-6.0)<1e-9;
}
static int builtin_test_tensor_cross_entropy(void){VM vm;init(&vm);Value d=va();ap(d.u.a,vf(0.0));ap(d.u.a,vf(2.0));Value sh=va();ap(sh.u.a,vi(2));Value x=tensor_from_array(&vm,3,(Value[]){d,sh,vb(1)});Value l=tensor_cross_entropy(&vm,2,(Value[]){x,vi(1)});if(!tensor_handle(l)||!tensor_backward(&vm,1,&l).u.b)return 0;Value g=tensor_grad(&vm,1,&x);return g.t==VARR&&g.u.a->n==2&&dn(g.u.a->v[1])<0&&dn(g.u.a->v[0])>0;}
static int builtin_test_tensor_matmul_adam(void){
    VM vm;init(&vm);Value ad=va();ap(ad.u.a,vf(1));ap(ad.u.a,vf(2));Value ash=va();ap(ash.u.a,vi(1));ap(ash.u.a,vi(2));Value bd=va();ap(bd.u.a,vf(3));ap(bd.u.a,vf(4));Value bsh=va();ap(bsh.u.a,vi(2));ap(bsh.u.a,vi(1));Value A=tensor_from_array(&vm,3,(Value[]){ad,ash,vb(1)});Value B=tensor_from_array(&vm,3,(Value[]){bd,bsh,vb(1)});if(!tensor_handle(A)||!tensor_handle(B))return 0;Value C=tensor_matmul(&vm,2,(Value[]){A,B});Value L=tensor_sum(&vm,1,&C);if(!tensor_backward(&vm,1,&L).u.b)return 0;Value ga=tensor_grad(&vm,1,&A),gb=tensor_grad(&vm,1,&B);if(ga.t!=VARR||gb.t!=VARR||ga.u.a->n!=2||gb.u.a->n!=2)return 0;if(fabs(dn(ga.u.a->v[0])-3)>1e-9||fabs(dn(ga.u.a->v[1])-4)>1e-9)return 0;if(fabs(dn(gb.u.a->v[0])-1)>1e-9||fabs(dn(gb.u.a->v[1])-2)>1e-9)return 0;return tensor_adam_step(&vm,2,(Value[]){A,vf(0.01)}).u.b;
}
static int builtin_test_longjmp_cleanup(void){
    VM vm;init(&vm);Fn*f=compile("error(\"boom\")","<longjmp-test>");(void)run(&vm,f,0,NULL);return vm.jmp_depth==0&&vm.err_depth==0;
}
static int builtin_test_llm_embedded_api(void){
    VM vm;init(&vm);Value i=ai_llm_info(&vm,0,NULL);if(i.t!=VSTRUCT)return 0;Value b=stget(i.u.st,"subprocess");return b.t==VBOOL&&!b.u.b;
}
static int builtin_test_api_validation(void){
    VM vm;init(&vm);Value r=api_request(&vm,2,(Value[]){vs("GET"),vs("http://127.0.0.1:1")});return r.t==VSTRUCT||r.t==VNULL;
}

static int builtin_test_security_hardening(void){
    VM vm;init(&vm);vm.module_caps=CAP_SYSTEM|CAP_WEB;vm.host_module_caps=CAP_SYSTEM|CAP_WEB;vm.module_sandboxed=1;
    Value inj=nsystem(&vm,1,(Value[]){vs("echo ok > /tmp/haris_injection_blocked")});if(inj.t!=VBOOL||inj.u.b)return 0;
    Value ok=nsystem(&vm,1,(Value[]){vs("true")});if(ok.t!=VINT||ok.u.i!=0)return 0;
    vm.sandbox_root="/tmp";if(fs_path_allowed(&vm,"../etc/passwd"))return 0;
    Value e=web_download(&vm,2,(Value[]){vs("http://127.0.0.1:1"),vs("../escape.bin")});if(e.t!=VBOOL||e.u.b)return 0;
    vm.module_sandboxed=1;vm.int_overflow=0;long long z=int_binop(&vm,I_ADD,LLONG_MAX,1);if(z!=0||!vm.int_overflow)return 0;
    vm.int_overflow=0;z=int_binop(&vm,I_MUL,LLONG_MAX,2);if(z!=0||!vm.int_overflow)return 0;return 1;
}


static int builtin_test_library_isolation(void){
    char base[PATH_MAX];if(!getcwd(base,sizeof base))return 0;
    char moddir[PATH_MAX];snprintf(moddir,sizeof moddir,"%s/haris_test_modules",base);mkdir(moddir,0700);
    char safe[PATH_MAX],priv[PATH_MAX],mainp[PATH_MAX];
    snprintf(safe,sizeof safe,"%s/safe.hr",moddir);snprintf(priv,sizeof priv,"%s/priv.hr",moddir);snprintf(mainp,sizeof mainp,"%s/main.hr",moddir);
    if(!write_text(safe,"export fn hello(x){ return \"hello \" + x }\n"))return 0;
    if(!write_text(priv,"# @capabilities: fs\nexport fn readme(){ return fs.read(\"./safe.hr\") }\n"))return 0;
    VM vm;init(&vm);char abs[PATH_MAX];if(!realpath(safe,abs))return 0;Value args[1]={vs(abs)};Value m=nimport(&vm,1,args);if(m.t!=VSTRUCT){remove(safe);remove(priv);rmdir(moddir);return 0;}
    Value fn=stget(m.u.st,"hello");if(fn.t!=VFN){remove(safe);remove(priv);rmdir(moddir);return 0;}
    VM vm2;init(&vm2);vm2.host_module_caps=0;Value pa[1]={vs(priv)};Value denied=nimport(&vm2,1,pa);int ok=(denied.t==VNULL||denied.t==VBOOL&&!denied.u.b);
    remove(safe);remove(priv);rmdir(moddir);return ok;
}
static int builtin_test_library_cache_caps(void){
    char base[PATH_MAX];if(!getcwd(base,sizeof base))return 0;char moddir[PATH_MAX];snprintf(moddir,sizeof moddir,"%s/haris_test_cache",base);mkdir(moddir,0700);char p[PATH_MAX];snprintf(p,sizeof p,"%s/cap.hr",moddir);
    if(!write_text(p,"# @capabilities: fs\nexport fn x(){ return 7 }\n"))return 0;
    char abs[PATH_MAX];if(!realpath(p,abs))return 0;VM a;init(&a);a.host_module_caps=CAP_FS;Value aa[1]={vs(abs)};Value x=nimport(&a,1,aa);VM b;init(&b);b.host_module_caps=0;Value bb[1]={vs(abs)};Value y=nimport(&b,1,bb);int ok=x.t==VSTRUCT&&(y.t==VNULL||y.t==VBOOL&&!y.u.b);remove(p);rmdir(moddir);return ok;
}


static int builtin_test_mixed_precision(void){VM vm;init(&vm);Value d=va();ap(d.u.a,vf(1.5));ap(d.u.a,vf(-2.25));Value sh=va();ap(sh.u.a,vi(2));Value t=tensor_from_array(&vm,3,(Value[]){d,sh,vb(0)});if(!tensor_handle(t))return 0;Value f=tensor_to_mixed(&vm,1,&t,1),b=tensor_to_mixed(&vm,1,&t,2);Value tf=tensor_from_mixed(&vm,1,&f),tb=tensor_from_mixed(&vm,1,&b);if(!tensor_handle(tf)||!tensor_handle(tb))return 0;return fabs(((HTensor*)tf.u.handle)->data[0]-1.5)<0.01&&fabs(((HTensor*)tb.u.handle)->data[1]+2.25)<0.02;}
static int builtin_test_cnn(void){VM vm;init(&vm);Value x=va();for(int i=0;i<3;i++){Value r=va();for(int j=0;j<3;j++)ap(r.u.a,vf((double)(i*3+j+1)));ap(x.u.a,r);}Value k=va();for(int i=0;i<2;i++){Value r=va();for(int j=0;j<2;j++)ap(r.u.a,vf(1));ap(k.u.a,r);}Value o=ai_conv2d(&vm,2,(Value[]){x,k});return o.t==VARR&&o.u.a->n==2&&o.u.a->v[0].u.a->n==2&&fabs(dn(o.u.a->v[0].u.a->v[0])-12)<1e-9;}
static int builtin_test_rnn_lstm_transformer(void){VM vm;init(&vm);Value r=ai_rnn(&vm,2,(Value[]){vi(3),vi(4)}),l=ai_lstm(&vm,2,(Value[]){vi(3),vi(4)}),tr=ai_transformer(&vm,3,(Value[]){vi(8),vi(2),vi(16)});if(r.t!=VSTRUCT||l.t!=VSTRUCT||tr.t!=VSTRUCT)return 0;Value seq=va();for(int t=0;t<3;t++){Value row=va();for(int j=0;j<3;j++)ap(row.u.a,vf(.1*(t+j+1)));ap(seq.u.a,row);}Value rr=ai_rnn_forward(&vm,2,(Value[]){r,seq}),ll=ai_lstm_forward(&vm,2,(Value[]){l,seq});Value s8=va();for(int t=0;t<3;t++){Value row=va();for(int j=0;j<8;j++)ap(row.u.a,vf(.01*(t+j+1)));ap(s8.u.a,row);}Value tt=ai_transformer_forward(&vm,2,(Value[]){tr,s8});return rr.t==VARR&&rr.u.a->n==3&&ll.t==VARR&&ll.u.a->n==3&&tt.t==VARR&&tt.u.a->n==3;}
static int builtin_test_model_zoo(void){VM vm;init(&vm);Value m=ai_model_zoo(&vm,1,(Value[]){vs("transformer_tiny")});Value r=ai_model_zoo(&vm,1,(Value[]){vs("rnn_tiny")});return m.t==VSTRUCT&&r.t==VSTRUCT&&stget(m.u.st,"kind").t==VSTR&&stget(r.u.st,"kind").t==VSTR;}
static int builtin_test_graph_opt(void){VM vm;init(&vm);Value xd=va();ap(xd.u.a,vf(1));ap(xd.u.a,vf(2));Value xsh=va();ap(xsh.u.a,vi(1));ap(xsh.u.a,vi(2));Value wd=va();for(int i=0;i<4;i++)ap(wd.u.a,vf(i==0||i==3?1.0:0.0));Value wsh=va();ap(wsh.u.a,vi(2));ap(wsh.u.a,vi(2));Value bd=va();ap(bd.u.a,vf(0));ap(bd.u.a,vf(0));Value bsh=va();ap(bsh.u.a,vi(2));Value x=tensor_from_array(&vm,3,(Value[]){xd,xsh,vb(0)}),w=tensor_from_array(&vm,3,(Value[]){wd,wsh,vb(0)}),b=tensor_from_array(&vm,3,(Value[]){bd,bsh,vb(0)});if(!tensor_handle(x)||!tensor_handle(w)||!tensor_handle(b))return 0;Value z=tensor_linear(&vm,3,(Value[]){x,w,b});if(!tensor_handle(z))return 0;Value q=tensor_relu(&vm,1,&z);if(!tensor_handle(q))return 0;Value rep=tensor_optimize_graph(&vm,1,&q);if(rep.t!=VSTRUCT||stget(rep.u.st,"optimized").t!=VBOOL||!stget(rep.u.st,"optimized").u.b)return 0;Value g=stget(rep.u.st,"graph");return tensor_handle(g);}
static int builtin_test_onnx_roundtrip(void){VM vm;init(&vm);Value m=ai_mlp(&vm,3,(Value[]){vi(2),vi(3),vi(2)});if(m.t!=VHANDLE)return 0;char p[]="/tmp/haris_test.onnx";if(!ai_onnx_export_mlp(&vm,2,(Value[]){m,vs(p)}).u.b)return 0;Value imp=ai_onnx_import(&vm,1,(Value[]){vs(p)});int ok=imp.t==VSTRUCT&&stget(imp.u.st,"format").t==VSTR&&stget(imp.u.st,"nodes").t==VARR&&stget(imp.u.st,"nodes").u.a->n>=5;if(ok){Value xr=va(),row=va();ap(row.u.a,vf(0.25));ap(row.u.a,vf(-0.5));ap(xr.u.a,row);Value y=ai_onnx_run(&vm,2,(Value[]){imp,xr});ok=(y.t==VARR&&y.u.a->n==1&&y.u.a->v[0].t==VARR&&y.u.a->v[0].u.a->n==2&&isfinite(dn(y.u.a->v[0].u.a->v[0]))&&isfinite(dn(y.u.a->v[0].u.a->v[1])));}remove(p);return ok;}
static int builtin_test_hf_validation(void){VM vm;init(&vm);Value a=ai_hf_info(&vm,1,(Value[]){vs("bad repo!")});return a.t==VNULL;}
static int builtin_test_cuda_api(void){VM vm;init(&vm);vm.module_sandboxed=0;Value i=ngpu_cuda_info(&vm,0,NULL);return i.t==VSTRUCT&&stget(i.u.st,"available").t==VBOOL;}
static int builtin_test_distributed_single(void){VM vm;init(&vm);Value d=distributed_init(&vm,3,(Value[]){vs("127.0.0.1:45991"),vi(0),vi(1)});if(d.t!=VHANDLE)return 0;Value v=va();ap(v.u.a,vf(2));Value r=distributed_allreduce(&vm,2,(Value[]){d,v});Value inf=distributed_info(&vm,1,&d);distributed_close(&vm,1,&d);return r.t==VARR&&r.u.a->n==1&&fabs(dn(r.u.a->v[0])-2)<1e-9&&inf.t==VSTRUCT;}
static int builtin_test_gpu_fallback(void){VM vm;init(&vm);Value a=va(),b=va();ap(a.u.a,vf(1));ap(a.u.a,vf(2));ap(b.u.a,vf(3));ap(b.u.a,vf(4));Value r=ngpu_cuda_add(&vm,2,(Value[]){a,b});return r.t==VARR&&r.u.a->n==2&&fabs(dn(r.u.a->v[1])-6)<1e-9;}


static int builtin_test_tls_profile(void){VM vm;init(&vm);Value p=web_tls_profile(&vm,0,NULL);return p.t==VSTRUCT&&stget(p.u.st,"openssl").t==VSTR&&stget(p.u.st,"tls12_ciphers").t==VSTR;}
static int builtin_test_http3_info(void){VM vm;init(&vm);Value p=web_http3_info(&vm,0,NULL);return p.t==VSTRUCT&&stget(p.u.st,"available").t==VBOOL;}
static int builtin_test_webrtc_info(void){VM vm;init(&vm);Value p=vsobj();/* no backend: query global availability must still be safe */return webrtc_available(&vm,0,NULL).t==VBOOL;}


static Value v91_test_predict(VM*vm,int n,Value*a){(void)vm;if(n!=2)return vn();return (a[0].t==VSTR&&a[1].t==VSTR)?vs("predicted"):vb(0);}
static int builtin_test_v91_multiplayer(void){
    VM vm;init(&vm);
    /* Rooms / lobby */
    Value rs=game_room_server_create(&vm,0,NULL);if(rs.t!=VHANDLE)return 0;
    if(!game_room_create(&vm,2,(Value[]){rs,vs("room1")}).u.b)return 0;
    if(!game_room_join(&vm,3,(Value[]){rs,vs("room1"),vs("alice")}).u.b)return 0;
    if(!game_room_join(&vm,3,(Value[]){rs,vs("room1"),vs("bob")}).u.b)return 0;
    if(game_room_players(&vm,2,(Value[]){rs,vs("room1")}).u.a->n!=2)return 0;
    if(!game_room_state_set(&vm,3,(Value[]){rs,vs("room1"),vs("state:v1")}).u.b)return 0;
    Value sy=game_room_state_sync(&vm,3,(Value[]){rs,vs("room1"),vi(0)});if(sy.t!=VSTRUCT||!stget(sy.u.st,"changed").u.b)return 0;
    /* Matchmaking / team balance */
    Value mm=game_matchmaker_create(&vm,2,(Value[]){vi(4),vi(2)});if(mm.t!=VHANDLE)return 0;
    game_matchmaker_enqueue(&vm,3,(Value[]){mm,vs("p1"),vf(100)});game_matchmaker_enqueue(&vm,3,(Value[]){mm,vs("p2"),vf(101)});game_matchmaker_enqueue(&vm,3,(Value[]){mm,vs("p3"),vf(99)});game_matchmaker_enqueue(&vm,3,(Value[]){mm,vs("p4"),vf(102)});
    Value mt=game_matchmaker_tick(&vm,1,&mm);if(mt.t!=VARR||mt.u.a->n!=1)return 0;Value teams=stget(mt.u.a->v[0].u.st,"teams");if(teams.t!=VARR||teams.u.a->n!=2||teams.u.a->v[0].u.a->n!=2||teams.u.a->v[1].u.a->n!=2)return 0;
    /* State replication: full + delta + prediction hook */
    Value tx=game_replication_create(&vm,0,NULL),rx=game_replication_create(&vm,0,NULL);if(tx.t!=VHANDLE||rx.t!=VHANDLE)return 0;
    Value s1=game_replication_snapshot(&vm,2,(Value[]){tx,vs("hello hello hello")});if(s1.t!=VSTRUCT)return 0;Value a1=game_replication_apply(&vm,2,(Value[]){rx,s1});if(a1.t!=VSTRUCT||stget(a1.u.st,"text").t!=VSTR||strcmp(stget(a1.u.st,"text").u.s,"hello hello hello"))return 0;
    Value s2=game_replication_snapshot(&vm,2,(Value[]){tx,vs("hello hello world")});if(s2.t!=VSTRUCT)return 0;Value a2=game_replication_apply(&vm,2,(Value[]){rx,s2});if(a2.t!=VSTRUCT||stget(a2.u.st,"text").t!=VSTR||strcmp(stget(a2.u.st,"text").u.s,"hello hello world"))return 0;
    Value hook={.t=VNATIVE,.u.native=v91_test_predict};Value pr=game_replication_predict(&vm,4,(Value[]){rx,vs("state"),vs("input"),hook});if(pr.t!=VSTR||strcmp(pr.u.s,"predicted"))return 0;
    /* Anti-cheat */
    Value ac=game_anticheat_create(&vm,0,NULL);if(ac.t!=VHANDLE)return 0;
    if(!game_anticheat_rate_limit(&vm,2,(Value[]){ac,vs("alice")} ).u.b)return 0;
    if(!game_anticheat_validate_move(&vm,6,(Value[]){ac,vs("alice"),vf(0),vf(0),vf(0),vf(0.016)}).u.b)return 0;
    if(game_anticheat_validate_move(&vm,6,(Value[]){ac,vs("alice"),vf(100),vf(0),vf(0),vf(0.016)}).u.b)return 0;
    if(!game_anticheat_authoritative(&vm,8,(Value[]){ac,vs("alice"),vf(0),vf(0),vf(0),vf(0.1),vf(0),vf(0)}).u.b)return 0;
    if(game_anticheat_authoritative(&vm,8,(Value[]){ac,vs("alice"),vf(0),vf(0),vf(0),vf(100),vf(0),vf(0)}).u.b)return 0;
    /* Reliable UDP, including fragmentation and ACK processing. */
    Value srv=udp_bind(&vm,2,(Value[]){vs("127.0.0.1"),vi(0)});if(srv.t!=VHANDLE)return 0;HUDP*su=udp_handle(srv);if(!su)return 0;struct sockaddr_storage ss;socklen_t sl=sizeof ss;if(getsockname(su->s,(struct sockaddr*)&ss,&sl)!=0)return 0;int port=ntohs(((struct sockaddr_in*)&ss)->sin_port);
    Value cli=udp_open(&vm,3,(Value[]){vs("127.0.0.1"),vi(port),vb(1)});if(cli.t!=VHANDLE)return 0;Value rels=net_reliable_open(&vm,1,(Value[]){srv}),relc=net_reliable_open(&vm,1,(Value[]){cli});if(rels.t!=VHANDLE||relc.t!=VHANDLE)return 0;
    char bigbuf[3001];for(int bi=0;bi<3000;bi++)bigbuf[bi]=(char)('A'+(bi%26));bigbuf[3000]=0;Value big=vs(bigbuf);if(net_reliable_send(&vm,2,(Value[]){relc,big}).t!=VINT)return 0;Value got=net_reliable_poll(&vm,2,(Value[]){rels,vi(1000)});if(got.t!=VSTRUCT)return 0;Value data=stget(got.u.st,"data");if(data.t!=VARR||data.u.a->n!=3000)return 0;for(size_t bi=0;bi<data.u.a->n;bi++)if(data.u.a->v[bi].t!=VINT||data.u.a->v[bi].u.i!=(long long)('A'+(bi%26)))return 0;net_reliable_poll(&vm,2,(Value[]){relc,vi(200)});Value inf=net_reliable_info(&vm,1,&relc);if(inf.t!=VSTRUCT)return 0;Value pst=stget(inf.u.st,"peer_stats");if(pst.t!=VARR||pst.u.a->n<1||stget(pst.u.a->v[0].u.st,"acked").u.i<1)return 0;
    net_reliable_close(&vm,1,&relc);net_reliable_close(&vm,1,&rels);game_anticheat_close(&vm,1,&ac);game_replication_close(&vm,1,&rx);game_replication_close(&vm,1,&tx);game_matchmaker_close(&vm,1,&mm);game_room_server_destroy(&vm,1,&rs);return 1;
}

static int builtin_test_namespace_depth(void){
    VM vm;init(&vm);int gi=egi(&vm.g,"net");if(gi<0||vm.g.v[gi].v.t!=VSTRUCT)return 0;Value rel=stget(vm.g.v[gi].v.u.st,"reliable");if(rel.t!=VSTRUCT)return 0;return stget(rel.u.st,"open").t==VNATIVE;
}

static int builtin_test_game_server_authority(void){
    VM vm;init(&vm);vm.host_module_caps|=CAP_NET|CAP_OS;
    Value s=game_server_create(&vm,3,(Value[]){vi(30),vi(8),vi(64)});if(s.t!=VHANDLE)return 0;
    Value a=game_server_join(&vm,2,(Value[]){s,vs("authority")});if(a.t!=VSTRUCT)return 0;
    if(!game_server_player_state(&vm,8,(Value[]){s,vs("authority"),vf(0),vf(0),vf(0),vf(100),vi(0),vi(1)}).u.b)return 0;
    uint64_t base=hr_now_ms();if(game_server_tick(&vm,2,(Value[]){s,vi((long long)base+34)}).t!=VINT)return 0;
    Value it=game_server_interpolate(&vm,3,(Value[]){s,vs("authority"),vi((long long)base+17)});if(it.t!=VSTRUCT)return 0;
    Value st=game_server_info(&vm,1,&s);if(st.t!=VSTRUCT)return 0;
    if(game_server_match_start(&vm,1,&s).t!=VINT)return 0;if(!game_server_match_end(&vm,2,(Value[]){s,vs("authority")}).u.b)return 0;
    Value mh=game_server_match_history(&vm,1,&s);if(mh.t!=VARR||mh.u.a->n<1)return 0;
    game_server_close(&vm,1,&s);return 1;
}

static int builtin_test_game_server_tick_clock(void){
    VM vm;init(&vm);vm.host_module_caps|=CAP_NET|CAP_OS;
    Value s=game_server_create(&vm,3,(Value[]){vi(60),vi(4),vi(8)});if(s.t!=VHANDLE)return 0;
    uint64_t base=1000000ULL;
    for(int i=0;i<61;i++)game_server_tick(&vm,2,(Value[]){s,vi((long long)(base+i*10ULL))});
    Value info=game_server_info(&vm,1,&s);
    int ok=info.t==VSTRUCT && stget(info.u.st,"tick").t==VINT && stget(info.u.st,"tick").u.i>=35 && stget(info.u.st,"tick").u.i<=37;
    game_server_close(&vm,1,&s);return ok;
}

static int builtin_test_v92_reliable_lifecycle(void){
    VM vm;init(&vm);
    Value srv=udp_bind(&vm,2,(Value[]){vs("127.0.0.1"),vi(0)});if(srv.t!=VHANDLE)return 0;
    Value inf=udp_info(&vm,1,&srv);if(inf.t!=VSTRUCT)return 0;Value pv=stget(inf.u.st,"port");if(pv.t!=VINT||pv.u.i<=0)return 0;
    Value cli=udp_open(&vm,3,(Value[]){vs("127.0.0.1"),pv,vb(1)});if(cli.t!=VHANDLE)return 0;
    Value rs=net_reliable_open(&vm,1,&srv),rc=net_reliable_open(&vm,1,&cli);if(rs.t!=VHANDLE||rc.t!=VHANDLE)return 0;
    if(net_reliable_send(&vm,2,(Value[]){rc,vs("lifecycle")}).t!=VINT)return 0;
    Value got=net_reliable_poll(&vm,2,(Value[]){rs,vi(1000)});if(got.t!=VSTRUCT)return 0;
    if(!net_reliable_tick(&vm,1,&rc).u.b)return 0;
    if(!net_reliable_close(&vm,1,&rc).u.b)return 0;
    if(net_reliable_close(&vm,1,&rc).u.b)return 0;
    if(net_reliable_info(&vm,1,&rc).t!=VNULL)return 0;
    if(!net_reliable_close(&vm,1,&rs).u.b)return 0;
    if(net_reliable_close(&vm,1,&rs).u.b)return 0;
    return udp_close(&vm,1,&cli).u.b && udp_close(&vm,1,&srv).u.b;
}

static int run_builtin_tests(int*total,int*passed){
    struct {const char*name;int(*fn)(void);} ts[]={
        {"web.batch transport result",builtin_test_web_batch_result},
        {"UDP loopback",builtin_test_udp_loopback},
        {"WebSocket framing",builtin_test_websocket_frame},
        {"Game AI brain/replay",builtin_test_game_ai},
        {"Game AI bot server integration",builtin_test_game_ai_server_bot},
        {"Game spectator + replay playback",builtin_test_game_spectator_replay},
        {"Game ranked MMR + leaderboard",builtin_test_game_ranked},
        {"Game secure admin command",builtin_test_game_admin_cmd},
        {"Game A* navigation",builtin_test_nav_astar},
        {"sandbox roundtrip",builtin_test_sandbox_roundtrip},
        {"gfx.draw_text",builtin_test_gfx_text},
        {"git.info parser",builtin_test_git_parser},
        {"data CSV",builtin_test_data_smoke},
        {"memory safe API",builtin_test_memory_safe},
        {"memory raw capability gate",builtin_test_memory_raw_gate},
        {"stale handle safety",builtin_test_handle_safety},
        {"math edge cases",builtin_test_math_edges},
        {"MLP fast inference",builtin_test_mlp_fast_path},
        {"MLP classification training",builtin_test_mlp_train_class},
        {"deep MLP",builtin_test_deep_mlp},
        {"engine scene graph",builtin_test_engine_scene},
        {"engine physics",builtin_test_engine_physics},
        {"GRC surface + JSON ownership",builtin_test_grc_surface},
        {"Tensor autograd",builtin_test_tensor_autograd},
        {"Tensor matmul + Adam",builtin_test_tensor_matmul_adam},
        {"Tensor cross entropy",builtin_test_tensor_cross_entropy},
        {"longjmp cleanup",builtin_test_longjmp_cleanup},
        {"in-process LLM API",builtin_test_llm_embedded_api},
        {"API validation",builtin_test_api_validation},
        {"TLS profile",builtin_test_tls_profile},
        {"HTTP/3 capability info",builtin_test_http3_info},
        {"WebRTC capability info",builtin_test_webrtc_info},
        {"security hardening",builtin_test_security_hardening},
        {"library isolation",builtin_test_library_isolation},
        {"library cache capability isolation",builtin_test_library_cache_caps},
        {"mixed precision",builtin_test_mixed_precision},
        {"CNN conv2d",builtin_test_cnn},
        {"RNN LSTM Transformer",builtin_test_rnn_lstm_transformer},
        {"model zoo",builtin_test_model_zoo},
        {"graph optimization",builtin_test_graph_opt},
        {"ONNX roundtrip",builtin_test_onnx_roundtrip},
        {"Hugging Face validation",builtin_test_hf_validation},
        {"CUDA API",builtin_test_cuda_api},
        {"distributed single rank",builtin_test_distributed_single},
        {"CUDA add/fallback",builtin_test_gpu_fallback},
        {"multiplayer stack v9.1",builtin_test_v91_multiplayer},
        {"nested namespace depth",builtin_test_namespace_depth},
        {"game server authority",builtin_test_game_server_authority},
        {"game server tick clock",builtin_test_game_server_tick_clock},
        {"HReliableUDP lifecycle v10.2",builtin_test_v92_reliable_lifecycle}
    };
    for(size_t i=0;i<sizeof(ts)/sizeof(ts[0]);i++){(*total)++;int ok=ts[i].fn();if(ok)(*passed)++;printf("%s %s\n",ok?"PASS":"FAIL",ts[i].name);}return *passed==*total?0:1;
}

static int cmd_test(const char*path){
    const char*tpath=(path&&*path)?path:"tests";
    int total=0,passed=0;
    if(!strcmp(tpath,"--builtin")){return run_builtin_tests(&total,&passed);}
    if((!path||!*path||!strcmp(tpath,"tests")) && access(tpath,F_OK)!=0){printf("No external tests found; running Haris built-in regression suite.\n");return run_builtin_tests(&total,&passed);}
    int discovery_rc=discover_and_run_tests(tpath,&total,&passed);
    if(total==0 && (!path || !*path || !strcmp(tpath,"tests"))){printf("No external tests found; running Haris built-in regression suite.\n");return run_builtin_tests(&total,&passed);}
    if(discovery_rc!=0 && total==0){fprintf(stderr,"haris test: test discovery failed\n");return 2;}
    if(total==0){fprintf(stderr,"haris test: no *.test.hr files found under '%s'\n",tpath);return 2;}
    printf("Tests: %d passed, %d failed\n",passed,total-passed);
    return passed==total?0:1;
}
static void bootstrap_stdlib(VM *vm);
static int cmd_repl(void){VM vm;init(&vm);bootstrap_stdlib(&vm);char line[4096];printf("Haris REPL %s. Type :quit to exit, :help for help.\n",HARIS_VERSION);for(;;){fputs("haris> ",stdout);fflush(stdout);if(!fgets(line,sizeof line,stdin))break;line[strcspn(line,"\r\n")]=0;if(!strcmp(line,":quit")||!strcmp(line,":q"))break;if(!strcmp(line,":help")){puts(":quit  exit\n:version  show version\nAny Haris expression/statement can be entered. Define functions with fn ... .");continue;}if(!strcmp(line,":version")){puts(HARIS_VERSION);continue;}if(!*line)continue;char*src=xmalloc(strlen(line)+2);strcpy(src,line);strcat(src,"\n");Fn*f=compile(src,"<repl>");xfree(src);Value r=run(&vm,f,0,0);if(r.t!=VNULL){pv(r);putchar('\n');}}return 0;}
static int cmd_debug(const char*path){char*src=readf(path);if(!src){fprintf(stderr,"haris debug: cannot open %s\n",path);return 2;}TV tv=lex(src);printf("Debug target: %s\nTokens: %d\n",path,tv.n);for(int i=0;i<tv.n&&i<200;i++)printf("%4d  line=%d  token=%d  %s\n",i,tv.v[i].line,tv.v[i].t,tv.v[i].s);Fn*f=compile(src,path);printf("Bytecode instructions: %d\n",f->ch.n);for(int i=0;i<f->ch.n&&i<200;i++)printf("%4d  op=%d a=%d line=%d\n",i,f->ch.v[i].op,f->ch.v[i].a,f->ch.v[i].line);/* debug allocations owned by runtime; keep them alive until process exit */(void)f;xfree(src);return 0;}
static unsigned cap_from_name(const char*s){
  if(!s)return 0;
  if(!strcmp(s,"os"))return CAP_OS;
  if(!strcmp(s,"system"))return CAP_SYSTEM;
  if(!strcmp(s,"net"))return CAP_NET;
  if(!strcmp(s,"web"))return CAP_WEB;
  if(!strcmp(s,"sql"))return CAP_SQL;
  if(!strcmp(s,"cloud"))return CAP_CLOUD;
  if(!strcmp(s,"git"))return CAP_GIT;
  if(!strcmp(s,"fs"))return CAP_FS;
  if(!strcmp(s,"nuclear"))return CAP_NUCLEAR;
  if(!strcmp(s,"defense"))return CAP_DEFENSE;if(!strcmp(s,"audit"))return CAP_DEFENSE;if(!strcmp(s,"redteam"))return CAP_REDTEAM;if(!strcmp(s,"pentest"))return CAP_PENTEST;if(!strcmp(s,"compliance"))return CAP_DEFENSE;if(!strcmp(s,"report"))return CAP_DEFENSE;
  if(!strcmp(s,"hw.cpu"))return CAP_HW_CPU;if(!strcmp(s,"hw.gpu"))return CAP_HW_GPU;if(!strcmp(s,"hw.board"))return CAP_HW_BOARD;if(!strcmp(s,"game.admin"))return CAP_GAME_ADMIN;
  return 0;
}
static void add_host_cap(VM*vm,const char*s){
  unsigned c=cap_from_name(s);
  if(!c)die("Haris: unknown module capability '%s'",s);
  vm->host_module_caps |= c;
}

static int run_sandbox_worker(const char *path,unsigned caps){
#ifdef HARIS_ANDROID
    (void)path;(void)caps;
    fprintf(stderr,"Haris Android: sandbox worker unavailable in-process.\n");
    return 3;
#else
    VM vm;init(&vm);vm.module_sandboxed=1;vm.sandbox_worker=1;vm.sandbox_auto=1;vm.module_caps=caps;vm.host_module_caps=caps;
    char abs[PATH_MAX];char root[PATH_MAX];char*src=NULL;
#ifdef _WIN32
    snprintf(abs,sizeof abs,"%s",path);
    src=readf(abs);if(!src){fprintf(stderr,"Haris worker: cannot open module %s\n",path);return 2;}
    snprintf(root,sizeof root,"%s",path);char *sl=strrchr(root,'\\');if(!sl)sl=strrchr(root,'/');if(sl&&sl!=root)*sl=0;
#else
    if(!realpath(path,abs))return 2;
    src=readf(abs);if(!src){fprintf(stderr,"Haris worker: cannot open module %s\n",path);return 2;}
    snprintf(root,sizeof root,"/tmp/haris-file-sbox-XXXXXX");
    if(!mkdtemp(root)){xfree(src);return 2;}
    if(chdir(root)!=0){xfree(src);remove_tree(root);fprintf(stderr,"Haris worker: cannot chdir to '%s': %s\n",root,strerror(errno));return 2;}
    vm.sandbox_root=root;
    extern char **environ;environ[0]=NULL;
#endif
    caps &= ~CAP_NUCLEAR;
    if(!sandbox_caps_safe(caps)){
        fprintf(stderr,"Haris security: hardware/raw capabilities are broker-only\n");
        xfree(src);
#ifndef _WIN32
        remove_tree(root);
#endif
        return 3;
    }
    if(!sandbox_platform_harden(caps,root,vm.sandbox_backend,sizeof vm.sandbox_backend) && sandbox_caps_are_privileged(caps)){
        fprintf(stderr,"Haris security: no native sandbox backend available; privileged capability denied\n");
        xfree(src);
#ifndef _WIN32
        remove_tree(root);
#endif
        return 3;
    }
    char why[160]={0};if(!module_scan(src,caps,why,sizeof why)){
        fprintf(stderr,"Haris worker: denied: %s\n",why);xfree(src);
#ifndef _WIN32
        remove_tree(root);
#endif
        return 3;
    }
    Fn*f=compile(src,abs);xfree(src);run(&vm,f,0,0);int rc=vm.last_error?1:0;
#ifndef _WIN32
    (void)remove_tree(root);
#endif
    return rc;
#endif
}

static const char*STD_STRING_HR="fn trim(s){ return string.trim(s) }\nfn find(s,x){ return string.find(s,x) }\nfn starts_with(s,x){ return string.starts_with(s,x) }\nfn ends_with(s,x){ return string.ends_with(s,x) }\nfn upper(s){ return string.upper(s) }\nfn lower(s){ return string.lower(s) }\n";
static const char*STD_ARRAY_HR="fn append(a,x){ return list.append(a,x) }\nfn pop(a){ return list.pop(a) }\n";
static const char*STD_MATH_HR="fn abs(x){ return math.abs(x) }\nfn sqrt(x){ return math.sqrt(x) }\nfn pow(x,y){ return math.pow(x,y) }\nfn min(x,y){ return math.min(x,y) }\nfn max(x,y){ return math.max(x,y) }\n";
static const char*STD_IO_HR="fn print_line(x){ print(x); return null }\n";
static void bootstrap_stdlib(VM *vm){
    const char*mods[][2]={{"std/string.hr",STD_STRING_HR},{"std/array.hr",STD_ARRAY_HR},{"std/math.hr",STD_MATH_HR},{"std/io.hr",STD_IO_HR}};
    for(size_t i=0;i<sizeof(mods)/sizeof(mods[0]);i++){Fn*f=compile(mods[i][1],mods[i][0]);(void)run(vm,f,0,0);}
}



#ifdef HARIS_ANDROID
/* Android native-host lifecycle helpers. The Android app process already supplies
   the OS-level sandbox; Haris additionally confines normal filesystem APIs to the
   app-private files directory when the JNI bridge sets a sandbox root. */
int haris_vm_set_sandbox_root(VM*vm,const char*root){
    if(!vm||!root||!*root)return 0;
    char *dup=(char*)malloc(strlen(root)+1);
    if(!dup)return 0;
    strcpy(dup,root);
    if(vm->sandbox_root_owned)free(vm->sandbox_root_owned);
    vm->sandbox_root_owned=dup;
    vm->sandbox_root=dup;
    return 1;
}
const char *haris_android_platform(void){return "android";}
int haris_android_is_supported(void){return 1;}
#endif

#ifdef HARIS_NO_MAIN
typedef struct HarisRuntime { VM vm; Fn *root; } HarisRuntime;
HarisRuntime *haris_runtime_new(void){HarisRuntime*r=(HarisRuntime*)calloc(1,sizeof(*r));if(!r)return NULL;init(&r->vm);bootstrap_stdlib(&r->vm);return r;}
int haris_runtime_load_string(HarisRuntime*r,const char*src,const char*name){if(!r||!src)return 0;r->root=compile(src,name?name:"<embedded>");run(&r->vm,r->root,0,NULL);return r->vm.last_error?0:1;}
int haris_runtime_load_file(HarisRuntime*r,const char*path){if(!r||!path)return 0;char*s=readf(path);if(!s)return 0;int ok=haris_runtime_load_string(r,s,path);xfree(s);return ok;}
int haris_runtime_set_f64(HarisRuntime*r,const char*k,double v){if(!r||!k)return 0;int i=egi(&r->vm.g,k);if(i>=0)r->vm.g.v[i].v=vf(v);else en(&r->vm.g,k,vf(v));return 1;}
int haris_runtime_set_i64(HarisRuntime*r,const char*k,long long v){if(!r||!k)return 0;int i=egi(&r->vm.g,k);if(i>=0)r->vm.g.v[i].v=vi(v);else en(&r->vm.g,k,vi(v));return 1;}
int haris_runtime_set_bool(HarisRuntime*r,const char*k,int v){if(!r||!k)return 0;int i=egi(&r->vm.g,k);if(i>=0)r->vm.g.v[i].v=vb(v);else en(&r->vm.g,k,vb(v));return 1;}
int haris_runtime_set_string(HarisRuntime*r,const char*k,const char*v){if(!r||!k)return 0;int i=egi(&r->vm.g,k);Value x=vs(v?v:"");if(i>=0)r->vm.g.v[i].v=x;else en(&r->vm.g,k,x);return 1;}
int haris_runtime_call(HarisRuntime*r,const char*fn,int argc,Value*args,Value*out){if(!r||!fn||argc<0||argc>H_CALL_MAX)return 0;int i=egi(&r->vm.g,fn);if(i<0||r->vm.g.v[i].v.t!=VFN)return 0;Value v=run(&r->vm,r->vm.g.v[i].v.u.fn,argc,args);if(out)*out=v;return r->vm.last_error?0:1;}
int haris_runtime_game_update(HarisRuntime*r,double dt){return haris_runtime_call(r,"_game_update",1,(Value[]){vf(dt)},NULL);}
int haris_runtime_set_module_caps(HarisRuntime*r,unsigned caps){if(!r)return 0;r->vm.host_module_caps=caps;r->vm.module_caps=caps;return 1;}
void haris_runtime_gc(HarisRuntime*r){if(r)gc_collect(&r->vm);}
void haris_runtime_free(HarisRuntime*r){if(!r)return;gc_collect(&r->vm);free(r);}
#endif

#ifdef __EMSCRIPTEN__
#define HARIS_API EMSCRIPTEN_KEEPALIVE
#else
#define HARIS_API
#endif
static HARIS_TLS char haris_api_json[262144];
HARIS_API const char*haris_version(void){return HARIS_VERSION;}
HARIS_API VM*haris_vm_create(void){VM*vm=(VM*)calloc(1,sizeof*vm);if(!vm)return NULL;init(vm);return vm;}
HARIS_API void haris_vm_destroy(VM*vm){if(!vm)return;gc_collect(vm);if(vm->sandbox_root_owned)free(vm->sandbox_root_owned);free(vm);}
HARIS_API const char*haris_eval_json(VM*vm,const char*src){if(!vm||!src)return NULL;char*copy=xdup(src);Fn*f=compile(copy,"<embed>");xfree(copy);Value r=run(vm,f,0,NULL);if(vm->last_error)return NULL;HJsonBuf b={0};if(!hjson_value(&b,r,0)){xfree(b.d);return NULL;}size_t n=b.n;if(n>=sizeof(haris_api_json))n=sizeof(haris_api_json)-1;memcpy(haris_api_json,b.d,n);haris_api_json[n]=0;xfree(b.d);return haris_api_json;}

#ifndef HARIS_EMBEDDED
int main(int argc,char**argv){g_program_path=argv[0];curl_global_init(CURL_GLOBAL_DEFAULT);VM vm;init(&vm);bootstrap_stdlib(&vm);
    /* A packed standalone binary contains its .hr payload after the native runtime.
       This is checked before normal CLI dispatch, so `main` behaves as the original program. */
    { char *packed_src=NULL; size_t packed_len=0; char self_path[PATH_MAX]={0}; if(haris_self_path(self_path,sizeof self_path,argv[0])){ int pr=haris_read_packed(self_path,&packed_src,&packed_len); if(pr==HARIS_PACK_INVALID){jit_shutdown();curl_global_cleanup();fprintf(stderr,"Haris: standalone package is corrupt or has an invalid payload\n");return 3;} if(pr==1){(void)packed_len;Fn*pf=compile(packed_src,self_path);xfree(packed_src);run(&vm,pf,0,NULL);int rr=vm.last_error?1:0;jit_shutdown();curl_global_cleanup();return rr;} } }long long cpu=0,mem=0;int cores=0,pct=100;char*profile=0;char*legal_doc=0;char*audit_log=0;int i=1;while(i<argc){if(!strcmp(argv[i],"--sandbox-stdin-worker")){unsigned cc=0;for(int j=1;j<argc;j++)if(!strcmp(argv[j],"--module-caps")&&j+1<argc)cc=(unsigned)strtoul(argv[j+1],0,10);int rr=run_sandbox_stdin_worker(cc);jit_shutdown();curl_global_cleanup();return rr;}if(!strcmp(argv[i],"--sandbox")&&i+1<argc){
    if(vm.host_module_caps&(CAP_REDTEAM|CAP_PENTEST)){
        if((vm.host_module_caps&CAP_REDTEAM)&&(!legal_doc||!authorize_redteam_document(&vm,legal_doc)))die("Haris: CAP_REDTEAM requires --legal-doc PATH");
        if((vm.host_module_caps&CAP_PENTEST)&&(!audit_log||!*audit_log)){die("Haris: CAP_PENTEST requires --audit-log PATH");}
        if((vm.host_module_caps&CAP_PENTEST)){vm.pentest_authorized=1;vm.audit_log_path=xdup(audit_log);if(!audit_log_event(&vm,"authorization",CAP_PENTEST))die("Haris: cannot open audit log");}
    }
    unsigned safe_caps=vm.host_module_caps & ~(CAP_NUCLEAR);int rr=spawn_sandboxed_module(argv[i+1],safe_caps);jit_shutdown();curl_global_cleanup();return rr;}if(!strcmp(argv[i],"--sandbox-worker")&&i+1<argc){unsigned cc=0;for(int j=2;j<argc;j++)if(!strcmp(argv[j],"--module-caps")&&j+1<argc)cc=(unsigned)strtoul(argv[j+1],0,10);int rr=run_sandbox_worker(argv[i+1],cc);jit_shutdown();curl_global_cleanup();return rr;}if(!strcmp(argv[i],"--trust-module")&&i+1<argc){add_trusted_module(&vm,argv[i+1]);i+=2;continue;}if(!strcmp(argv[i],"--version")){puts(HARIS_VERSION);jit_shutdown();curl_global_cleanup();return 0;}if(!strcmp(argv[i],"-h")||!strcmp(argv[i],"--help")){usage();jit_shutdown();curl_global_cleanup();return 0;}if(!strcmp(argv[i],"--max-cpu-ms")&&i+1<argc){cpu=atoll(argv[++i]);i++;continue;}if(!strcmp(argv[i],"--max-memory-mb")&&i+1<argc){mem=atoll(argv[++i]);i++;continue;}if(!strcmp(argv[i],"--cpu-cores")&&i+1<argc){cores=atoi(argv[++i]);i++;continue;}if(!strcmp(argv[i],"--cpu-percent")&&i+1<argc){pct=atoi(argv[++i]);i++;continue;}if(!strcmp(argv[i],"--profile")&&i+1<argc){profile=xdup(argv[++i]);i++;continue;}if(!strcmp(argv[i],"--allow-module-cap")&&i+1<argc){add_host_cap(&vm,argv[++i]);i++;continue;}if(!strcmp(argv[i],"--legal-doc")&&i+1<argc){legal_doc=xdup(argv[++i]);i++;continue;}if(!strcmp(argv[i],"--audit-log")&&i+1<argc){audit_log=xdup(argv[++i]);i++;continue;}break;}if(cpu<0||mem<0||cores<0||pct<1||pct>100)die("invalid resource limit");if((vm.host_module_caps&CAP_REDTEAM)!=0){if(!legal_doc||!authorize_redteam_document(&vm,legal_doc))die("Haris: CAP_REDTEAM requires --legal-doc PATH with readable SHA-256 document");}if((vm.host_module_caps&CAP_PENTEST)!=0){if(!audit_log||!*audit_log)die("Haris: CAP_PENTEST requires --audit-log PATH");vm.pentest_authorized=1;vm.audit_log_path=xdup(audit_log);if(!audit_log_event(&vm,"authorization",CAP_PENTEST))die("Haris: cannot open audit log");}if(cpu)vm.cpu_limit_ms=cpu;if(mem){vm.mem_limit_bytes=(size_t)mem*1024u*1024u;vm.memory_auto=0;}if(cores)vm.cpu_cores=cores;vm.cpu_percent=pct;vm.cpu_slice_wall_ms=0;vm.cpu_slice_cpu_ms=0;if(profile)vm.profile=profile;if(!apply_cpu_affinity(&vm))fprintf(stderr,"Haris: warning: CPU affinity request could not be applied\n");g_mem_limit=vm.mem_limit_bytes;if(i>=argc){usage();jit_shutdown();curl_global_cleanup();return 0;}if(!strcmp(argv[i],"pack")){if(i+1>=argc){fprintf(stderr,"usage: haris pack <main.hr> [output]\n");jit_shutdown();curl_global_cleanup();return 2;}int rc=cmd_pack(argv[i+1],i+2<argc?argv[i+2]:NULL,argv[0]);jit_shutdown();curl_global_cleanup();return rc;}if(!strcmp(argv[i],"game")&&i+1<argc&&!strcmp(argv[i+1],"init")){int rc=cmd_game_init(i+2<argc?argv[i+2]:NULL);jit_shutdown();curl_global_cleanup();return rc;}if(!strcmp(argv[i],"init")){int rc=cmd_init(i+1<argc?argv[i+1]:NULL);jit_shutdown();curl_global_cleanup();return rc;}if(!strcmp(argv[i],"add")||!strcmp(argv[i],"install")){int rc=i+1<argc?package_edit(argv[i+1],0):2;jit_shutdown();curl_global_cleanup();return rc;}if(!strcmp(argv[i],"remove")||!strcmp(argv[i],"uninstall")){int rc=i+1<argc?package_edit(argv[i+1],1):2;jit_shutdown();curl_global_cleanup();return rc;}if(!strcmp(argv[i],"update")){int rc=package_update();jit_shutdown();curl_global_cleanup();return rc;}if(!strcmp(argv[i],"search")){int rc=i+1<argc?package_search(argv[i+1]):2;jit_shutdown();curl_global_cleanup();return rc;}if(!strcmp(argv[i],"stats")){int rc=registry_stats_cli();jit_shutdown();curl_global_cleanup();return rc;}if(!strcmp(argv[i],"publish")){if(i+3>=argc){fprintf(stderr,"usage: haris publish <name> <version> <main.hr> [description]\n");jit_shutdown();curl_global_cleanup();return 2;}int rc=registry_publish_cli(argv[i+1],argv[i+2],argv[i+3],i+4<argc?argv[i+4]:"");jit_shutdown();curl_global_cleanup();return rc;}if(!strcmp(argv[i],"lock")){int rc=package_lock();jit_shutdown();curl_global_cleanup();return rc;}if(!strcmp(argv[i],"test")){int rc=cmd_test(i+1<argc?argv[i+1]:"tests");jit_shutdown();curl_global_cleanup();return rc;}if(!strcmp(argv[i],"arena")&&i+1<argc&&!strcmp(argv[i+1],"demo")){int rc=cmd_arena_demo();jit_shutdown();curl_global_cleanup();return rc;}if(!strcmp(argv[i],"bench")){int rc=cmd_bench(i+1<argc?argc-(i+1):0,argv+i+1);jit_shutdown();curl_global_cleanup();return rc;}if(!strcmp(argv[i],"repl")){int rc=cmd_repl();jit_shutdown();curl_global_cleanup();return rc;}if(!strcmp(argv[i],"debug")){int rc=i+1<argc?cmd_debug(argv[i+1]):2;jit_shutdown();curl_global_cleanup();return rc;}if(argc-i==2&&!strcmp(argv[i],"-e")){run(&vm,compile(argv[i+1],"<cmd>"),0,0);int rr=vm.last_error?1:0;jit_shutdown();curl_global_cleanup();return rr;}if(argc-i!=1){usage();jit_shutdown();curl_global_cleanup();return 2;}char*src=readf(argv[i]);if(!src)die("Haris: cannot open %s",argv[i]);Fn*f=compile(src,argv[i]);xfree(src);run(&vm,f,0,0);int rr=vm.last_error?1:0;jit_shutdown();curl_global_cleanup();return rr;}

#endif /* HARIS_EMBEDDED */
