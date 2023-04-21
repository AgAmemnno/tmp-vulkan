namespace vi_test
{
   struct VertexInput{
       int location;
       blender::gpu::shader::Type type;
       std::string  name;
   };

   struct VertexInfo{
       std::vector<VertexInput> inputs;
       std::string         debugPrintf;
       std::string         shaderUsed;
   };

   const int test_all = 47;

   VertexInfo  TestData[test_all] = {
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f  \", v_0 );",
          " basic_depth_mesh.vert.glsl \n  basic_depth_mesh_clipped.vert.glsl \n  basic_depth_mesh_conservative_no_geom.vert.glsl \n  basic_depth_mesh_conservative_no_geom_clipped.vert.glsl \n  gpu_shader_3D_clipped_uniform_color.vert.glsl \n  gpu_shader_3D_depth_only.vert.glsl \n  gpu_shader_3D_depth_only_clipped.vert.glsl \n  gpu_shader_3D_line_dashed_uniform_color.vert.glsl \n  gpu_shader_3D_line_dashed_uniform_color_clipped.vert.glsl \n  gpu_shader_3D_point_uniform_size_uniform_color_aa.vert.glsl \n  gpu_shader_3D_point_uniform_size_uniform_color_aa_clipped.vert.glsl \n  gpu_shader_3D_uniform_color.vert.glsl \n  gpu_shader_3D_uniform_color_clipped.vert.glsl \n  overlay_depth_only.vert.glsl \n  overlay_depth_only_clipped.vert.glsl \n  overlay_edit_mesh_depth.vert.glsl \n  overlay_edit_mesh_depth_clipped.vert.glsl \n  overlay_edit_uv_mask_image.vert.glsl \n  overlay_edit_uv_stencil_image.vert.glsl \n  overlay_edit_uv_tiled_image_borders.vert.glsl \n  overlay_extra_loose_point.vert.glsl \n  overlay_extra_loose_point_clipped.vert.glsl \n  overlay_extra_point.vert.glsl \n  overlay_extra_point_clipped.vert.glsl \n  overlay_facing.vert.glsl \n  overlay_facing_clipped.vert.glsl \n  overlay_grid.vert.glsl \n  overlay_grid_background.vert.glsl \n  overlay_grid_image.vert.glsl \n  overlay_image.vert.glsl \n  overlay_image_clipped.vert.glsl \n  overlay_outline_prepass_mesh.vert.glsl \n  overlay_outline_prepass_mesh_clipped.vert.glsl \n  overlay_uniform_color.vert.glsl \n  overlay_uniform_color_clipped.vert.glsl \n  select_id_uniform.vert.glsl \n  select_id_uniform_clipped.vert.glsl \n  workbench_volume_object_closest_coba_no_slice.vert.glsl \n  workbench_volume_object_closest_no_coba_no_slice.vert.glsl \n  workbench_volume_object_cubic_coba_no_slice.vert.glsl \n  workbench_volume_object_cubic_no_coba_no_slice.vert.glsl \n  workbench_volume_object_linear_coba_no_slice.vert.glsl \n  workbench_volume_object_linear_no_coba_no_slice.vert.glsl \n  workbench_volume_smoke_closest_coba_no_slice.vert.glsl \n  workbench_volume_smoke_closest_no_coba_no_slice.vert.glsl \n  workbench_volume_smoke_cubic_coba_no_slice.vert.glsl \n  workbench_volume_smoke_cubic_no_coba_no_slice.vert.glsl \n  workbench_volume_smoke_linear_coba_no_slice.vert.glsl \n  workbench_volume_smoke_linear_no_coba_no_slice.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC4,"v_0" },
          { 1,blender::gpu::shader::Type::VEC3,"v_1" },
          { 2,blender::gpu::shader::Type::VEC3,"v_2" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v4f , v_1 %v3f , v_2 %v3f  \", v_0 , v_1 , v_2 );",
          " basic_depth_pointcloud.vert.glsl \n  basic_depth_pointcloud_clipped.vert.glsl \n  basic_depth_pointcloud_conservative_no_geom.vert.glsl \n  basic_depth_pointcloud_conservative_no_geom_clipped.vert.glsl \n  overlay_outline_prepass_pointcloud.vert.glsl \n  overlay_outline_prepass_pointcloud_clipped.vert.glsl \n  overlay_uniform_color_pointcloud.vert.glsl \n  overlay_uniform_color_pointcloud_clipped.vert.glsl \n  overlay_viewer_attribute_pointcloud.vert.glsl \n  overlay_viewer_attribute_pointcloud_clipped.vert.glsl \n  workbench_opaque_ptcloud_tex_none_clip.vert.glsl \n  workbench_opaque_ptcloud_tex_none_no_clip.vert.glsl \n  workbench_opaque_ptcloud_tex_single_clip.vert.glsl \n  workbench_opaque_ptcloud_tex_single_no_clip.vert.glsl \n  workbench_opaque_ptcloud_tex_tile_clip.vert.glsl \n  workbench_opaque_ptcloud_tex_tile_no_clip.vert.glsl \n  workbench_transp_flat_ptcloud_tex_none_clip.vert.glsl \n  workbench_transp_flat_ptcloud_tex_none_no_clip.vert.glsl \n  workbench_transp_flat_ptcloud_tex_single_clip.vert.glsl \n  workbench_transp_flat_ptcloud_tex_single_no_clip.vert.glsl \n  workbench_transp_flat_ptcloud_tex_tile_clip.vert.glsl \n  workbench_transp_flat_ptcloud_tex_tile_no_clip.vert.glsl \n  workbench_transp_matcap_ptcloud_tex_none_clip.vert.glsl \n  workbench_transp_matcap_ptcloud_tex_none_no_clip.vert.glsl \n  workbench_transp_matcap_ptcloud_tex_single_clip.vert.glsl \n  workbench_transp_matcap_ptcloud_tex_single_no_clip.vert.glsl \n  workbench_transp_matcap_ptcloud_tex_tile_clip.vert.glsl \n  workbench_transp_matcap_ptcloud_tex_tile_no_clip.vert.glsl \n  workbench_transp_studio_ptcloud_tex_none_clip.vert.glsl \n  workbench_transp_studio_ptcloud_tex_none_no_clip.vert.glsl \n  workbench_transp_studio_ptcloud_tex_single_clip.vert.glsl \n  workbench_transp_studio_ptcloud_tex_single_no_clip.vert.glsl \n  workbench_transp_studio_ptcloud_tex_tile_clip.vert.glsl \n  workbench_transp_studio_ptcloud_tex_tile_no_clip.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::VEC3,"v_1" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %v3f  \", v_0 , v_1 );",
          " eevee_legacy_cryptomatte_mesh.vert.glsl \n  eevee_legacy_shader_shadow.vert.glsl \n  gpu_shader_simple_lighting.vert.glsl \n  overlay_extra_groundline.vert.glsl \n  overlay_extra_groundline_clipped.vert.glsl \n  overlay_paint_vertcol.vert.glsl \n  overlay_paint_vertcol_clipped.vert.glsl \n  overlay_paint_wire_clipped.vert.glsl \n  workbench_volume_object_closest_coba_slice.vert.glsl \n  workbench_volume_object_closest_no_coba_slice.vert.glsl \n  workbench_volume_object_cubic_coba_slice.vert.glsl \n  workbench_volume_object_cubic_no_coba_slice.vert.glsl \n  workbench_volume_object_linear_coba_slice.vert.glsl \n  workbench_volume_object_linear_no_coba_slice.vert.glsl \n  workbench_volume_smoke_closest_coba_slice.vert.glsl \n  workbench_volume_smoke_closest_no_coba_slice.vert.glsl \n  workbench_volume_smoke_cubic_coba_slice.vert.glsl \n  workbench_volume_smoke_cubic_no_coba_slice.vert.glsl \n  workbench_volume_smoke_linear_coba_slice.vert.glsl \n  workbench_volume_smoke_linear_no_coba_slice.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::VEC3,"v_1" },
          { 2,blender::gpu::shader::Type::VEC3,"v_2" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %v3f , v_2 %v3f  \", v_0 , v_1 , v_2 );",
          " eevee_legacy_effect_motion_blur_object.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::INT,"v_1" },
          { 2,blender::gpu::shader::Type::MAT4,"v_2" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %i , v_2 %v4f,%v4f,%v4f,%v4f  \", v_0 , v_1 , v_2[0],v_2[1], v_2[2], v_2[3] );",
          " eevee_legacy_probe_planar_display.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC2,"v_0" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v2f  \", v_0 );",
          " eevee_legacy_studiolight_background.vert.glsl \n  eevee_legacy_studiolight_probe.vert.glsl \n  gpu_shader_2D_area_borders.vert.glsl \n  gpu_shader_2D_checker.vert.glsl \n  gpu_shader_2D_diag_stripes.vert.glsl \n  gpu_shader_2D_image_overlays_stereo_merge.vert.glsl \n  gpu_shader_2D_point_uniform_size_uniform_color_aa.vert.glsl \n  gpu_shader_2D_point_uniform_size_uniform_color_outline_aa.vert.glsl \n  gpu_shader_icon_multi.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 15,blender::gpu::shader::Type::INT,"v_15" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_15 %i  \", v_0 , v_15 );",
          " eevee_shadow_tag_usage_transparent.vert.glsl \n "
   },
   {
          {
          { 15,blender::gpu::shader::Type::INT,"v_15" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_15 %i  \", v_15 );",
          " eevee_surface_deferred_curves.vert.glsl \n  eevee_surface_deferred_gpencil.vert.glsl \n  eevee_surface_deferred_world.vert.glsl \n  eevee_surface_depth_curves.vert.glsl \n  eevee_surface_depth_gpencil.vert.glsl \n  eevee_surface_depth_world.vert.glsl \n  eevee_surface_forward_curves.vert.glsl \n  eevee_surface_forward_gpencil.vert.glsl \n  eevee_surface_forward_world.vert.glsl \n  eevee_surface_shadow_curves.vert.glsl \n  eevee_surface_shadow_gpencil.vert.glsl \n  eevee_surface_shadow_world.vert.glsl \n  eevee_surface_world_curves.vert.glsl \n  eevee_surface_world_gpencil.vert.glsl \n  eevee_surface_world_world.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::VEC3,"v_1" },
          { 15,blender::gpu::shader::Type::INT,"v_15" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %v3f , v_15 %i  \", v_0 , v_1 , v_15 );",
          " eevee_surface_deferred_mesh.vert.glsl \n  eevee_surface_depth_mesh.vert.glsl \n  eevee_surface_forward_mesh.vert.glsl \n  eevee_surface_shadow_mesh.vert.glsl \n  eevee_surface_world_mesh.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC2,"v_0" },
          { 1,blender::gpu::shader::Type::VEC2,"v_1" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v2f , v_1 %v2f  \", v_0 , v_1 );",
          " gpu_shader_2D_image_desaturate_color.vert.glsl \n  gpu_shader_2D_image_overlays_merge.vert.glsl \n  gpu_shader_2D_image_shuffle_color.vert.glsl \n  gpu_shader_cycles_display_fallback.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC2,"v_0" },
          { 1,blender::gpu::shader::Type::VEC2,"v_1" },
          { 2,blender::gpu::shader::Type::VEC2,"v_2" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v2f , v_1 %v2f , v_2 %v2f  \", v_0 , v_1 , v_2 );",
          " gpu_shader_2D_nodelink.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC2,"v_0" },
          { 1,blender::gpu::shader::Type::VEC2,"v_1" },
          { 2,blender::gpu::shader::Type::VEC2,"v_2" },
          { 3,blender::gpu::shader::Type::VEC2,"v_3" },
          { 4,blender::gpu::shader::Type::VEC2,"v_4" },
          { 5,blender::gpu::shader::Type::VEC2,"v_5" },
          { 6,blender::gpu::shader::Type::VEC2,"v_6" },
          { 7,blender::gpu::shader::Type::UVEC4,"v_7" },
          { 8,blender::gpu::shader::Type::VEC4,"v_8" },
          { 9,blender::gpu::shader::Type::VEC4,"v_9" },
          { 10,blender::gpu::shader::Type::UVEC2,"v_10" },
          { 11,blender::gpu::shader::Type::FLOAT,"v_11" },
          { 12,blender::gpu::shader::Type::FLOAT,"v_12" },
          { 13,blender::gpu::shader::Type::FLOAT,"v_13" },
          { 14,blender::gpu::shader::Type::FLOAT,"v_14" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v2f , v_1 %v2f , v_2 %v2f , v_3 %v2f , v_4 %v2f , v_5 %v2f , v_6 %v2f , v_7 %v4u , v_8 %v4f , v_9 %v4f , v_10 %v2u , v_11 %f , v_12 %f , v_13 %f , v_14 %f  \", v_0 , v_1 , v_2 , v_3 , v_4 , v_5 , v_6 , v_7 , v_8 , v_9 , v_10 , v_11 , v_12 , v_13 , v_14 );",
          " gpu_shader_2D_nodelink_inst.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC2,"v_0" },
          { 1,blender::gpu::shader::Type::FLOAT,"v_1" },
          { 2,blender::gpu::shader::Type::VEC4,"v_2" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v2f , v_1 %f , v_2 %v4f  \", v_0 , v_1 , v_2 );",
          " gpu_shader_2D_point_varying_size_varying_color.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::UINT,"v_0" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %u  \", v_0 );",
          " gpu_shader_2D_widget_shadow.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::VEC4,"v_1" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %v4f  \", v_0 , v_1 );",
          " gpu_shader_3D_flat_color.vert.glsl \n  gpu_shader_3D_flat_color_clipped.vert.glsl \n  gpu_shader_3D_smooth_color.vert.glsl \n  gpu_shader_3D_smooth_color_clipped.vert.glsl \n  overlay_armature_wire.vert.glsl \n  overlay_armature_wire_clipped.vert.glsl \n  overlay_paint_face.vert.glsl \n  overlay_paint_face_clipped.vert.glsl \n  overlay_paint_point.vert.glsl \n  overlay_paint_point_clipped.vert.glsl \n  overlay_paint_wire.vert.glsl \n  overlay_viewer_attribute_curve.vert.glsl \n  overlay_viewer_attribute_curve_clipped.vert.glsl \n  overlay_viewer_attribute_mesh.vert.glsl \n  overlay_viewer_attribute_mesh_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::VEC2,"v_1" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %v2f  \", v_0 , v_1 );",
          " gpu_shader_3D_image.vert.glsl \n  gpu_shader_3D_image_color.vert.glsl \n  overlay_paint_texture.vert.glsl \n  overlay_paint_texture_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::VEC4,"v_1" },
          { 2,blender::gpu::shader::Type::FLOAT,"v_2" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %v4f , v_2 %f  \", v_0 , v_1 , v_2 );",
          " gpu_shader_3D_point_varying_size_varying_color.vert.glsl \n  overlay_particle_dot.vert.glsl \n  overlay_particle_dot_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::VEC4,"v_1" },
          { 2,blender::gpu::shader::Type::FLOAT,"v_2" },
          { 3,blender::gpu::shader::Type::MAT4,"v_3" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %v4f , v_2 %f , v_3 %v4f,%v4f,%v4f,%v4f  \", v_0 , v_1 , v_2 , v_3[0],v_3[1], v_3[2], v_3[3] );",
          " gpu_shader_instance_varying_color_varying_size.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC4,"v_0" },
          { 1,blender::gpu::shader::Type::VEC4,"v_1" },
          { 2,blender::gpu::shader::Type::VEC2,"v_2" },
          { 3,blender::gpu::shader::Type::FLOAT,"v_3" },
          { 4,blender::gpu::shader::Type::UINT,"v_4" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v4f , v_1 %v4f , v_2 %v2f , v_3 %f , v_4 %u  \", v_0 , v_1 , v_2 , v_3 , v_4 );",
          " gpu_shader_keyframe_shape.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC4,"v_0" },
          { 1,blender::gpu::shader::Type::VEC4,"v_1" },
          { 2,blender::gpu::shader::Type::IVEC2,"v_2" },
          { 3,blender::gpu::shader::Type::INT,"v_3" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v4f , v_1 %v4f , v_2 %v2i , v_3 %i  \", v_0 , v_1 , v_2 , v_3 );",
          " gpu_shader_text.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::IVEC2,"v_0" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v2i  \", v_0 );",
          " image_engine_color_shader.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::IVEC2,"v_0" },
          { 1,blender::gpu::shader::Type::VEC2,"v_1" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v2i , v_1 %v2f  \", v_0 , v_1 );",
          " image_engine_depth_shader.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC2,"v_0" },
          { 1,blender::gpu::shader::Type::VEC4,"v_1" },
          { 2,blender::gpu::shader::Type::MAT4,"v_2" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v2f , v_1 %v4f , v_2 %v4f,%v4f,%v4f,%v4f  \", v_0 , v_1 , v_2[0],v_2[1], v_2[2], v_2[3] );",
          " overlay_armature_dof_solid.vert.glsl \n  overlay_armature_dof_solid_clipped.vert.glsl \n  overlay_armature_dof_wire.vert.glsl \n  overlay_armature_dof_wire_clipped.vert.glsl \n  overlay_armature_sphere_solid.vert.glsl \n  overlay_armature_sphere_solid_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC2,"v_0" },
          { 1,blender::gpu::shader::Type::VEC2,"v_1" },
          { 2,blender::gpu::shader::Type::VEC2,"v_2" },
          { 3,blender::gpu::shader::Type::VEC4,"v_3" },
          { 4,blender::gpu::shader::Type::VEC4,"v_4" },
          { 5,blender::gpu::shader::Type::VEC4,"v_5" },
          { 6,blender::gpu::shader::Type::VEC3,"v_6" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v2f , v_1 %v2f , v_2 %v2f , v_3 %v4f , v_4 %v4f , v_5 %v4f , v_6 %v3f  \", v_0 , v_1 , v_2 , v_3 , v_4 , v_5 , v_6 );",
          " overlay_armature_envelope_outline.vert.glsl \n  overlay_armature_envelope_outline_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::VEC4,"v_1" },
          { 2,blender::gpu::shader::Type::VEC4,"v_2" },
          { 3,blender::gpu::shader::Type::VEC3,"v_3" },
          { 4,blender::gpu::shader::Type::VEC3,"v_4" },
          { 5,blender::gpu::shader::Type::VEC3,"v_5" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %v4f , v_2 %v4f , v_3 %v3f , v_4 %v3f , v_5 %v3f  \", v_0 , v_1 , v_2 , v_3 , v_4 , v_5 );",
          " overlay_armature_envelope_solid.vert.glsl \n  overlay_armature_envelope_solid_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::VEC3,"v_1" },
          { 2,blender::gpu::shader::Type::MAT4,"v_2" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %v3f , v_2 %v4f,%v4f,%v4f,%v4f  \", v_0 , v_1 , v_2[0],v_2[1], v_2[2], v_2[3] );",
          " overlay_armature_shape_solid.vert.glsl \n  overlay_armature_shape_solid_clipped.vert.glsl \n  overlay_armature_shape_wire.vert.glsl \n  overlay_armature_shape_wire_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC2,"v_0" },
          { 1,blender::gpu::shader::Type::MAT4,"v_1" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v2f , v_1 %v4f,%v4f,%v4f,%v4f  \", v_0 , v_1[0],v_1[1], v_1[2], v_1[3] );",
          " overlay_armature_sphere_outline.vert.glsl \n  overlay_armature_sphere_outline_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC2,"v_0" },
          { 1,blender::gpu::shader::Type::UINT,"v_1" },
          { 2,blender::gpu::shader::Type::VEC3,"v_2" },
          { 3,blender::gpu::shader::Type::VEC3,"v_3" },
          { 4,blender::gpu::shader::Type::VEC4,"v_4" },
          { 5,blender::gpu::shader::Type::VEC4,"v_5" },
          { 6,blender::gpu::shader::Type::VEC4,"v_6" },
          { 7,blender::gpu::shader::Type::VEC4,"v_7" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v2f , v_1 %u , v_2 %v3f , v_3 %v3f , v_4 %v4f , v_5 %v4f , v_6 %v4f , v_7 %v4f  \", v_0 , v_1 , v_2 , v_3 , v_4 , v_5 , v_6 , v_7 );",
          " overlay_armature_stick.vert.glsl \n  overlay_armature_stick_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::UINT,"v_1" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %u  \", v_0 , v_1 );",
          " overlay_edit_curve_point.vert.glsl \n  overlay_edit_curve_point_clipped.vert.glsl \n  overlay_edit_gpencil_guide_point.vert.glsl \n  overlay_edit_gpencil_guide_point_clipped.vert.glsl \n  overlay_edit_lattice_point.vert.glsl \n  overlay_edit_lattice_point_clipped.vert.glsl \n  overlay_motion_path_point.vert.glsl \n  overlay_motion_path_point_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::VEC3,"v_1" },
          { 2,blender::gpu::shader::Type::VEC3,"v_2" },
          { 3,blender::gpu::shader::Type::FLOAT,"v_3" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %v3f , v_2 %v3f , v_3 %f  \", v_0 , v_1 , v_2 , v_3 );",
          " overlay_edit_curve_wire.vert.glsl \n  overlay_edit_curve_wire_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::INT,"v_1" },
          { 2,blender::gpu::shader::Type::UINT,"v_2" },
          { 3,blender::gpu::shader::Type::FLOAT,"v_3" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %i , v_2 %u , v_3 %f  \", v_0 , v_1 , v_2 , v_3 );",
          " overlay_edit_gpencil_point.vert.glsl \n  overlay_edit_gpencil_point_clipped.vert.glsl \n  overlay_edit_gpencil_wire.vert.glsl \n  overlay_edit_gpencil_wire_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::FLOAT,"v_1" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %f  \", v_0 , v_1 );",
          " overlay_edit_lattice_wire.vert.glsl \n  overlay_edit_lattice_wire_clipped.vert.glsl \n  overlay_edit_mesh_analysis.vert.glsl \n  overlay_edit_mesh_analysis_clipped.vert.glsl \n  overlay_edit_particle_point.vert.glsl \n  overlay_edit_particle_point_clipped.vert.glsl \n  overlay_edit_particle_strand.vert.glsl \n  overlay_edit_particle_strand_clipped.vert.glsl \n  overlay_sculpt_curves_cage.vert.glsl \n  overlay_sculpt_curves_cage_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::UVEC4,"v_1" },
          { 2,blender::gpu::shader::Type::VEC3,"v_2" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %v4u , v_2 %v3f  \", v_0 , v_1 , v_2 );",
          " overlay_edit_mesh_face.vert.glsl \n  overlay_edit_mesh_face_clipped.vert.glsl \n  overlay_edit_mesh_vert.vert.glsl \n  overlay_edit_mesh_vert_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::UVEC4,"v_1" },
          { 2,blender::gpu::shader::Type::VEC4,"v_2" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %v4u , v_2 %v4f  \", v_0 , v_1 , v_2 );",
          " overlay_edit_mesh_facedot.vert.glsl \n  overlay_edit_mesh_facedot_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::VEC4,"v_1" },
          { 2,blender::gpu::shader::Type::VEC4,"v_2" },
          { 3,blender::gpu::shader::Type::VEC4,"v_3" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %v4f , v_2 %v4f , v_3 %v4f  \", v_0 , v_1 , v_2 , v_3 );",
          " overlay_edit_mesh_normal.vert.glsl \n  overlay_edit_mesh_normal_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::FLOAT,"v_1" },
          { 2,blender::gpu::shader::Type::VEC3,"v_2" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %f , v_2 %v3f  \", v_0 , v_1 , v_2 );",
          " overlay_edit_mesh_skin_root.vert.glsl \n  overlay_edit_mesh_skin_root_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC2,"v_0" },
          { 1,blender::gpu::shader::Type::UINT,"v_1" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v2f , v_1 %u  \", v_0 , v_1 );",
          " overlay_edit_uv_faces.vert.glsl \n  overlay_edit_uv_face_dots.vert.glsl \n  overlay_edit_uv_verts.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC2,"v_0" },
          { 1,blender::gpu::shader::Type::VEC2,"v_1" },
          { 2,blender::gpu::shader::Type::FLOAT,"v_2" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v2f , v_1 %v2f , v_2 %f  \", v_0 , v_1 , v_2 );",
          " overlay_edit_uv_stretching_angle.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC2,"v_0" },
          { 1,blender::gpu::shader::Type::FLOAT,"v_1" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v2f , v_1 %f  \", v_0 , v_1 );",
          " overlay_edit_uv_stretching_area.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::INT,"v_1" },
          { 2,blender::gpu::shader::Type::VEC4,"v_2" },
          { 3,blender::gpu::shader::Type::MAT4,"v_3" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %i , v_2 %v4f , v_3 %v4f,%v4f,%v4f,%v4f  \", v_0 , v_1 , v_2 , v_3[0],v_3[1], v_3[2], v_3[3] );",
          " overlay_extra.vert.glsl \n  overlay_extra_clipped.vert.glsl \n  overlay_extra_select.vert.glsl \n  overlay_extra_select_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::VEC4,"v_1" },
          { 2,blender::gpu::shader::Type::INT,"v_2" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %v4f , v_2 %i  \", v_0 , v_1 , v_2 );",
          " overlay_extra_wire.vert.glsl \n  overlay_extra_wire_clipped.vert.glsl \n  overlay_extra_wire_object.vert.glsl \n  overlay_extra_wire_object_clipped.vert.glsl \n  overlay_extra_wire_select.vert.glsl \n  overlay_extra_wire_select_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::FLOAT,"v_0" },
          { 1,blender::gpu::shader::Type::VEC3,"v_1" },
          { 2,blender::gpu::shader::Type::VEC3,"v_2" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %f , v_1 %v3f , v_2 %v3f  \", v_0 , v_1 , v_2 );",
          " overlay_paint_weight.vert.glsl \n  overlay_paint_weight_clipped.vert.glsl \n  overlay_paint_weight_fake_shading.vert.glsl \n  overlay_paint_weight_fake_shading_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::VEC4,"v_1" },
          { 2,blender::gpu::shader::Type::FLOAT,"v_2" },
          { 3,blender::gpu::shader::Type::VEC3,"v_3" },
          { 4,blender::gpu::shader::Type::INT,"v_4" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %v4f , v_2 %f , v_3 %v3f , v_4 %i  \", v_0 , v_1 , v_2 , v_3 , v_4 );",
          " overlay_particle_shape.vert.glsl \n  overlay_particle_shape_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::VEC3,"v_1" },
          { 2,blender::gpu::shader::Type::FLOAT,"v_2" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %v3f , v_2 %f  \", v_0 , v_1 , v_2 );",
          " overlay_sculpt_mask.vert.glsl \n  overlay_sculpt_mask_clipped.vert.glsl \n  overlay_wireframe.vert.glsl \n  overlay_wireframe_clipped.vert.glsl \n  overlay_wireframe_custom_depth.vert.glsl \n  overlay_wireframe_custom_depth_clipped.vert.glsl \n  overlay_wireframe_select.vert.glsl \n  overlay_wireframe_select_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::INT,"v_1" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %i  \", v_0 , v_1 );",
          " select_id_flat.vert.glsl \n  select_id_flat_clipped.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::VEC3,"v_1" },
          { 2,blender::gpu::shader::Type::VEC4,"v_2" },
          { 3,blender::gpu::shader::Type::VEC2,"v_3" },
          { 15,blender::gpu::shader::Type::IVEC2,"v_15" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %v3f , v_2 %v4f , v_3 %v2f , v_15 %v2i  \", v_0 , v_1 , v_2 , v_3 , v_15 );",
          " workbench_next_prepass_curves_opaque_flat_material_clip.vert.glsl \n  workbench_next_prepass_curves_opaque_flat_material_no_clip.vert.glsl \n  workbench_next_prepass_curves_opaque_flat_texture_clip.vert.glsl \n  workbench_next_prepass_curves_opaque_flat_texture_no_clip.vert.glsl \n  workbench_next_prepass_curves_opaque_flat_vertex_clip.vert.glsl \n  workbench_next_prepass_curves_opaque_flat_vertex_no_clip.vert.glsl \n  workbench_next_prepass_curves_opaque_matcap_material_clip.vert.glsl \n  workbench_next_prepass_curves_opaque_matcap_material_no_clip.vert.glsl \n  workbench_next_prepass_curves_opaque_matcap_texture_clip.vert.glsl \n  workbench_next_prepass_curves_opaque_matcap_texture_no_clip.vert.glsl \n  workbench_next_prepass_curves_opaque_matcap_vertex_clip.vert.glsl \n  workbench_next_prepass_curves_opaque_matcap_vertex_no_clip.vert.glsl \n  workbench_next_prepass_curves_opaque_studio_material_clip.vert.glsl \n  workbench_next_prepass_curves_opaque_studio_material_no_clip.vert.glsl \n  workbench_next_prepass_curves_opaque_studio_texture_clip.vert.glsl \n  workbench_next_prepass_curves_opaque_studio_texture_no_clip.vert.glsl \n  workbench_next_prepass_curves_opaque_studio_vertex_clip.vert.glsl \n  workbench_next_prepass_curves_opaque_studio_vertex_no_clip.vert.glsl \n  workbench_next_prepass_curves_transparent_flat_material_clip.vert.glsl \n  workbench_next_prepass_curves_transparent_flat_material_no_clip.vert.glsl \n  workbench_next_prepass_curves_transparent_flat_texture_clip.vert.glsl \n  workbench_next_prepass_curves_transparent_flat_texture_no_clip.vert.glsl \n  workbench_next_prepass_curves_transparent_flat_vertex_clip.vert.glsl \n  workbench_next_prepass_curves_transparent_flat_vertex_no_clip.vert.glsl \n  workbench_next_prepass_curves_transparent_matcap_material_clip.vert.glsl \n  workbench_next_prepass_curves_transparent_matcap_material_no_cl.vert.glsl \n  workbench_next_prepass_curves_transparent_matcap_texture_clip.vert.glsl \n  workbench_next_prepass_curves_transparent_matcap_texture_no_cli.vert.glsl \n  workbench_next_prepass_curves_transparent_matcap_vertex_clip.vert.glsl \n  workbench_next_prepass_curves_transparent_matcap_vertex_no_clip.vert.glsl \n  workbench_next_prepass_curves_transparent_studio_material_clip.vert.glsl \n  workbench_next_prepass_curves_transparent_studio_material_no_cl.vert.glsl \n  workbench_next_prepass_curves_transparent_studio_texture_clip.vert.glsl \n  workbench_next_prepass_curves_transparent_studio_texture_no_cli.vert.glsl \n  workbench_next_prepass_curves_transparent_studio_vertex_clip.vert.glsl \n  workbench_next_prepass_curves_transparent_studio_vertex_no_clip.vert.glsl \n  workbench_next_prepass_mesh_opaque_flat_material_clip.vert.glsl \n  workbench_next_prepass_mesh_opaque_flat_material_no_clip.vert.glsl \n  workbench_next_prepass_mesh_opaque_flat_texture_clip.vert.glsl \n  workbench_next_prepass_mesh_opaque_flat_texture_no_clip.vert.glsl \n  workbench_next_prepass_mesh_opaque_flat_vertex_clip.vert.glsl \n  workbench_next_prepass_mesh_opaque_flat_vertex_no_clip.vert.glsl \n  workbench_next_prepass_mesh_opaque_matcap_material_clip.vert.glsl \n  workbench_next_prepass_mesh_opaque_matcap_material_no_clip.vert.glsl \n  workbench_next_prepass_mesh_opaque_matcap_texture_clip.vert.glsl \n  workbench_next_prepass_mesh_opaque_matcap_texture_no_clip.vert.glsl \n  workbench_next_prepass_mesh_opaque_matcap_vertex_clip.vert.glsl \n  workbench_next_prepass_mesh_opaque_matcap_vertex_no_clip.vert.glsl \n  workbench_next_prepass_mesh_opaque_studio_material_clip.vert.glsl \n  workbench_next_prepass_mesh_opaque_studio_material_no_clip.vert.glsl \n  workbench_next_prepass_mesh_opaque_studio_texture_clip.vert.glsl \n  workbench_next_prepass_mesh_opaque_studio_texture_no_clip.vert.glsl \n  workbench_next_prepass_mesh_opaque_studio_vertex_clip.vert.glsl \n  workbench_next_prepass_mesh_opaque_studio_vertex_no_clip.vert.glsl \n  workbench_next_prepass_mesh_transparent_flat_material_clip.vert.glsl \n  workbench_next_prepass_mesh_transparent_flat_material_no_clip.vert.glsl \n  workbench_next_prepass_mesh_transparent_flat_texture_clip.vert.glsl \n  workbench_next_prepass_mesh_transparent_flat_texture_no_clip.vert.glsl \n  workbench_next_prepass_mesh_transparent_flat_vertex_clip.vert.glsl \n  workbench_next_prepass_mesh_transparent_flat_vertex_no_clip.vert.glsl \n  workbench_next_prepass_mesh_transparent_matcap_material_clip.vert.glsl \n  workbench_next_prepass_mesh_transparent_matcap_material_no_clip.vert.glsl \n  workbench_next_prepass_mesh_transparent_matcap_texture_clip.vert.glsl \n  workbench_next_prepass_mesh_transparent_matcap_texture_no_clip.vert.glsl \n  workbench_next_prepass_mesh_transparent_matcap_vertex_clip.vert.glsl \n  workbench_next_prepass_mesh_transparent_matcap_vertex_no_clip.vert.glsl \n  workbench_next_prepass_mesh_transparent_studio_material_clip.vert.glsl \n  workbench_next_prepass_mesh_transparent_studio_material_no_clip.vert.glsl \n  workbench_next_prepass_mesh_transparent_studio_texture_clip.vert.glsl \n  workbench_next_prepass_mesh_transparent_studio_texture_no_clip.vert.glsl \n  workbench_next_prepass_mesh_transparent_studio_vertex_clip.vert.glsl \n  workbench_next_prepass_mesh_transparent_studio_vertex_no_clip.vert.glsl \n  workbench_next_prepass_ptcloud_opaque_flat_material_clip.vert.glsl \n  workbench_next_prepass_ptcloud_opaque_flat_material_no_clip.vert.glsl \n  workbench_next_prepass_ptcloud_opaque_flat_texture_clip.vert.glsl \n  workbench_next_prepass_ptcloud_opaque_flat_texture_no_clip.vert.glsl \n  workbench_next_prepass_ptcloud_opaque_flat_vertex_clip.vert.glsl \n  workbench_next_prepass_ptcloud_opaque_flat_vertex_no_clip.vert.glsl \n  workbench_next_prepass_ptcloud_opaque_matcap_material_clip.vert.glsl \n  workbench_next_prepass_ptcloud_opaque_matcap_material_no_clip.vert.glsl \n  workbench_next_prepass_ptcloud_opaque_matcap_texture_clip.vert.glsl \n  workbench_next_prepass_ptcloud_opaque_matcap_texture_no_clip.vert.glsl \n  workbench_next_prepass_ptcloud_opaque_matcap_vertex_clip.vert.glsl \n  workbench_next_prepass_ptcloud_opaque_matcap_vertex_no_clip.vert.glsl \n  workbench_next_prepass_ptcloud_opaque_studio_material_clip.vert.glsl \n  workbench_next_prepass_ptcloud_opaque_studio_material_no_clip.vert.glsl \n  workbench_next_prepass_ptcloud_opaque_studio_texture_clip.vert.glsl \n  workbench_next_prepass_ptcloud_opaque_studio_texture_no_clip.vert.glsl \n  workbench_next_prepass_ptcloud_opaque_studio_vertex_clip.vert.glsl \n  workbench_next_prepass_ptcloud_opaque_studio_vertex_no_clip.vert.glsl \n  workbench_next_prepass_ptcloud_transparent_flat_material_clip.vert.glsl \n  workbench_next_prepass_ptcloud_transparent_flat_material_no_cli.vert.glsl \n  workbench_next_prepass_ptcloud_transparent_flat_texture_clip.vert.glsl \n  workbench_next_prepass_ptcloud_transparent_flat_texture_no_clip.vert.glsl \n  workbench_next_prepass_ptcloud_transparent_flat_vertex_clip.vert.glsl \n  workbench_next_prepass_ptcloud_transparent_flat_vertex_no_clip.vert.glsl \n  workbench_next_prepass_ptcloud_transparent_matcap_material_clip.vert.glsl \n  workbench_next_prepass_ptcloud_transparent_matcap_material_no_c.vert.glsl \n  workbench_next_prepass_ptcloud_transparent_matcap_texture_clip.vert.glsl \n  workbench_next_prepass_ptcloud_transparent_matcap_texture_no_cl.vert.glsl \n  workbench_next_prepass_ptcloud_transparent_matcap_vertex_clip.vert.glsl \n  workbench_next_prepass_ptcloud_transparent_matcap_vertex_no_cli.vert.glsl \n  workbench_next_prepass_ptcloud_transparent_studio_material_clip.vert.glsl \n  workbench_next_prepass_ptcloud_transparent_studio_material_no_c.vert.glsl \n  workbench_next_prepass_ptcloud_transparent_studio_texture_clip.vert.glsl \n  workbench_next_prepass_ptcloud_transparent_studio_texture_no_cl.vert.glsl \n  workbench_next_prepass_ptcloud_transparent_studio_vertex_clip.vert.glsl \n  workbench_next_prepass_ptcloud_transparent_studio_vertex_no_cli.vert.glsl \n "
   },
   {
          {
          { 0,blender::gpu::shader::Type::VEC3,"v_0" },
          { 1,blender::gpu::shader::Type::VEC3,"v_1" },
          { 2,blender::gpu::shader::Type::VEC4,"v_2" },
          { 3,blender::gpu::shader::Type::VEC2,"v_3" },
          },
          "debugPrintfEXT(\"VertexInput Checkers  v_0 %v3f , v_1 %v3f , v_2 %v4f , v_3 %v2f  \", v_0 , v_1 , v_2 , v_3 );",
          " workbench_opaque_mesh_tex_none_clip.vert.glsl \n  workbench_opaque_mesh_tex_none_no_clip.vert.glsl \n  workbench_opaque_mesh_tex_single_clip.vert.glsl \n  workbench_opaque_mesh_tex_single_no_clip.vert.glsl \n  workbench_opaque_mesh_tex_tile_clip.vert.glsl \n  workbench_opaque_mesh_tex_tile_no_clip.vert.glsl \n  workbench_transp_flat_mesh_tex_none_clip.vert.glsl \n  workbench_transp_flat_mesh_tex_none_no_clip.vert.glsl \n  workbench_transp_flat_mesh_tex_single_clip.vert.glsl \n  workbench_transp_flat_mesh_tex_single_no_clip.vert.glsl \n  workbench_transp_flat_mesh_tex_tile_clip.vert.glsl \n  workbench_transp_flat_mesh_tex_tile_no_clip.vert.glsl \n  workbench_transp_matcap_mesh_tex_none_clip.vert.glsl \n  workbench_transp_matcap_mesh_tex_none_no_clip.vert.glsl \n  workbench_transp_matcap_mesh_tex_single_clip.vert.glsl \n  workbench_transp_matcap_mesh_tex_single_no_clip.vert.glsl \n  workbench_transp_matcap_mesh_tex_tile_clip.vert.glsl \n  workbench_transp_matcap_mesh_tex_tile_no_clip.vert.glsl \n  workbench_transp_studio_mesh_tex_none_clip.vert.glsl \n  workbench_transp_studio_mesh_tex_none_no_clip.vert.glsl \n  workbench_transp_studio_mesh_tex_single_clip.vert.glsl \n  workbench_transp_studio_mesh_tex_single_no_clip.vert.glsl \n  workbench_transp_studio_mesh_tex_tile_clip.vert.glsl \n  workbench_transp_studio_mesh_tex_tile_no_clip.vert.glsl \n "
   },
   };
}//namespace vi_test


