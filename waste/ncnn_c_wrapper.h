// АВТОГЕНЕРАЦІЯ ЧЕРЕЗ PYTHON. НЕ РЕДАГУВАТИ!
#ifndef NCNN_C_WRAPPER_H
#define NCNN_C_WRAPPER_H

#ifdef _WIN32
    #define EXPORT_C __declspec(dllexport)
#else
    #define EXPORT_C __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Оголошуємо непрозорі структури (Opaque pointers) для С-коду
typedef struct ncnn_net_t ncnn_net_t;
typedef struct ncnn_mat_t ncnn_mat_t;
typedef struct ncnn_extractor_t ncnn_extractor_t;
typedef struct ncnn_command_t ncnn_command_t;

// --- ЖИТТЄВИЙ ЦИКЛ ОБ'ЄКТІВ ---
EXPORT_C ncnn_net_t* ncnn_net_create();
EXPORT_C void ncnn_net_destroy(ncnn_net_t* net);

EXPORT_C ncnn_mat_t* ncnn_mat_create();
EXPORT_C void ncnn_mat_destroy(ncnn_mat_t* mat);

EXPORT_C ncnn_extractor_t* ncnn_extractor_create(ncnn_net_t* net);
EXPORT_C void ncnn_extractor_destroy(ncnn_extractor_t* ex);


// === Методи класу Net ===
EXPORT_C void ncnn_net_set_vulkan_device(ncnn_net_t* self, int device_index);
EXPORT_C void ncnn_net_set_vulkan_device(ncnn_net_t* self, const VulkanDevice* vkdev);
EXPORT_C VulkanDevice* ncnn_net_vulkan_device(ncnn_net_t* self);
EXPORT_C int ncnn_net_register_custom_layer(ncnn_net_t* self, const char* type, layer_creator_func creator, layer_destroyer_func destroyer = 0, void* userdata = 0);
EXPORT_C int ncnn_net_custom_layer_to_index(ncnn_net_t* self, const char* type);
EXPORT_C int ncnn_net_register_custom_layer(ncnn_net_t* self, int index, layer_creator_func creator, layer_destroyer_func destroyer = 0, void* userdata = 0);
EXPORT_C int ncnn_net_load_param(ncnn_net_t* self, const DataReader& dr);
EXPORT_C int ncnn_net_load_param_bin(ncnn_net_t* self, const DataReader& dr);
EXPORT_C int ncnn_net_load_model(ncnn_net_t* self, const DataReader& dr);
EXPORT_C int ncnn_net_load_param(ncnn_net_t* self, FILE* fp);
EXPORT_C int ncnn_net_load_param(ncnn_net_t* self, const char* protopath);
EXPORT_C int ncnn_net_load_param_mem(ncnn_net_t* self, const char* mem);
EXPORT_C int ncnn_net_load_param_bin(ncnn_net_t* self, FILE* fp);
EXPORT_C int ncnn_net_load_param_bin(ncnn_net_t* self, const char* protopath);
EXPORT_C int ncnn_net_load_model(ncnn_net_t* self, FILE* fp);
EXPORT_C int ncnn_net_load_model(ncnn_net_t* self, const char* modelpath);
EXPORT_C int ncnn_net_load_param(ncnn_net_t* self, const unsigned char* mem);
EXPORT_C int ncnn_net_load_model(ncnn_net_t* self, const unsigned char* mem);
EXPORT_C int ncnn_net_load_param(ncnn_net_t* self, AAsset* asset);
EXPORT_C int ncnn_net_load_param(ncnn_net_t* self, AAssetManager* mgr, const char* assetpath);
EXPORT_C int ncnn_net_load_param_bin(ncnn_net_t* self, AAsset* asset);
EXPORT_C int ncnn_net_load_param_bin(ncnn_net_t* self, AAssetManager* mgr, const char* assetpath);
EXPORT_C int ncnn_net_load_model(ncnn_net_t* self, AAsset* asset);
EXPORT_C int ncnn_net_load_model(ncnn_net_t* self, AAssetManager* mgr, const char* assetpath);
EXPORT_C void ncnn_net_clear(ncnn_net_t* self);
EXPORT_C Extractor ncnn_net_create_extractor(ncnn_net_t* self);
EXPORT_C std::vector<int>& ncnn_net_input_indexes(ncnn_net_t* self);
EXPORT_C std::vector<int>& ncnn_net_output_indexes(ncnn_net_t* self);
EXPORT_C char*>& ncnn_net_input_names(ncnn_net_t* self);
EXPORT_C char*>& ncnn_net_output_names(ncnn_net_t* self);
EXPORT_C std::vector<Blob>& ncnn_net_blobs(ncnn_net_t* self);
EXPORT_C std::vector<Layer*>& ncnn_net_layers(ncnn_net_t* self);
EXPORT_C std::vector<Blob>& ncnn_net_mutable_blobs(ncnn_net_t* self);
EXPORT_C std::vector<Layer*>& ncnn_net_mutable_layers(ncnn_net_t* self);
EXPORT_C int ncnn_net_find_blob_index_by_name(ncnn_net_t* self, const char* name);
EXPORT_C int ncnn_net_find_layer_index_by_name(ncnn_net_t* self, const char* name);
EXPORT_C Layer* ncnn_net_create_custom_layer(ncnn_net_t* self, const char* type);
EXPORT_C Layer* ncnn_net_create_custom_layer(ncnn_net_t* self, int index);

// === Методи класу Mat ===
EXPORT_C void ncnn_mat_fill(ncnn_mat_t* self, float v);
EXPORT_C void ncnn_mat_fill(ncnn_mat_t* self, int v);
EXPORT_C void ncnn_mat_fill(ncnn_mat_t* self, float32x4_t _v);
EXPORT_C void ncnn_mat_fill(ncnn_mat_t* self, uint16x4_t _v);
EXPORT_C void ncnn_mat_fill(ncnn_mat_t* self, int32x4_t _v);
EXPORT_C void ncnn_mat_fill(ncnn_mat_t* self, int32x4_t _v0, int32x4_t _v1);
EXPORT_C void ncnn_mat_fill(ncnn_mat_t* self, float16x4_t _v);
EXPORT_C void ncnn_mat_fill(ncnn_mat_t* self, float16x8_t _v);
EXPORT_C void ncnn_mat_fill(ncnn_mat_t* self, __m512 _v);
EXPORT_C void ncnn_mat_fill(ncnn_mat_t* self, __m256 _v, int i = 0);
EXPORT_C void ncnn_mat_fill(ncnn_mat_t* self, __m128 _v);
EXPORT_C void ncnn_mat_fill(ncnn_mat_t* self, __m128i _v);
EXPORT_C void ncnn_mat_fill(ncnn_mat_t* self, v4f32 _v);
EXPORT_C void ncnn_mat_fill(ncnn_mat_t* self, vfloat32m1_t _v);
EXPORT_C void ncnn_mat_fill(ncnn_mat_t* self, vuint16m1_t _v);
EXPORT_C void ncnn_mat_fill(ncnn_mat_t* self, vint8m1_t _v);
EXPORT_C void ncnn_mat_fill(ncnn_mat_t* self, vfloat16m1_t _v);
EXPORT_C void ncnn_mat_fill(ncnn_mat_t* self, T v);
EXPORT_C ncnn_mat_t* ncnn_mat_clone(ncnn_mat_t* self, Allocator* allocator = 0);
EXPORT_C void ncnn_mat_clone_from(ncnn_mat_t* self, const ncnn::Mat& mat, Allocator* allocator = 0);
EXPORT_C ncnn_mat_t* ncnn_mat_reshape(ncnn_mat_t* self, int w, Allocator* allocator = 0);
EXPORT_C ncnn_mat_t* ncnn_mat_reshape(ncnn_mat_t* self, int w, int h, Allocator* allocator = 0);
EXPORT_C ncnn_mat_t* ncnn_mat_reshape(ncnn_mat_t* self, int w, int h, int c, Allocator* allocator = 0);
EXPORT_C ncnn_mat_t* ncnn_mat_reshape(ncnn_mat_t* self, int w, int h, int d, int c, Allocator* allocator = 0);
EXPORT_C void ncnn_mat_create(ncnn_mat_t* self, int w, size_t elemsize = 4u, Allocator* allocator = 0);
EXPORT_C void ncnn_mat_create(ncnn_mat_t* self, int w, int h, size_t elemsize = 4u, Allocator* allocator = 0);
EXPORT_C void ncnn_mat_create(ncnn_mat_t* self, int w, int h, int c, size_t elemsize = 4u, Allocator* allocator = 0);
EXPORT_C void ncnn_mat_create(ncnn_mat_t* self, int w, int h, int d, int c, size_t elemsize = 4u, Allocator* allocator = 0);
EXPORT_C void ncnn_mat_create(ncnn_mat_t* self, int w, size_t elemsize, int elempack, Allocator* allocator = 0);
EXPORT_C void ncnn_mat_create(ncnn_mat_t* self, int w, int h, size_t elemsize, int elempack, Allocator* allocator = 0);
EXPORT_C void ncnn_mat_create(ncnn_mat_t* self, int w, int h, int c, size_t elemsize, int elempack, Allocator* allocator = 0);
EXPORT_C void ncnn_mat_create(ncnn_mat_t* self, int w, int h, int d, int c, size_t elemsize, int elempack, Allocator* allocator = 0);
EXPORT_C void ncnn_mat_create_like(ncnn_mat_t* self, const Mat& m, Allocator* allocator = 0);
EXPORT_C void ncnn_mat_create_like(ncnn_mat_t* self, const VkMat& m, Allocator* allocator = 0);
EXPORT_C void ncnn_mat_create_like(ncnn_mat_t* self, const VkImageMat& im, Allocator* allocator = 0);
EXPORT_C void ncnn_mat_addref(ncnn_mat_t* self);
EXPORT_C void ncnn_mat_release(ncnn_mat_t* self);
EXPORT_C bool ncnn_mat_empty(ncnn_mat_t* self);
EXPORT_C size_t ncnn_mat_total(ncnn_mat_t* self);
EXPORT_C int ncnn_mat_elembits(ncnn_mat_t* self);
EXPORT_C ncnn_mat_t* ncnn_mat_shape(ncnn_mat_t* self);
EXPORT_C ncnn_mat_t* ncnn_mat_channel(ncnn_mat_t* self, int c);
EXPORT_C ncnn_mat_t* ncnn_mat_channel(ncnn_mat_t* self, int c);
EXPORT_C ncnn_mat_t* ncnn_mat_depth(ncnn_mat_t* self, int z);
EXPORT_C ncnn_mat_t* ncnn_mat_depth(ncnn_mat_t* self, int z);
EXPORT_C float* ncnn_mat_row(ncnn_mat_t* self, int y);
EXPORT_C float* ncnn_mat_row(ncnn_mat_t* self, int y);
EXPORT_C T* ncnn_mat_row(ncnn_mat_t* self, int y);
EXPORT_C T* ncnn_mat_row(ncnn_mat_t* self, int y);
EXPORT_C ncnn_mat_t* ncnn_mat_channel_range(ncnn_mat_t* self, int c, int channels);
EXPORT_C ncnn_mat_t* ncnn_mat_channel_range(ncnn_mat_t* self, int c, int channels);
EXPORT_C ncnn_mat_t* ncnn_mat_depth_range(ncnn_mat_t* self, int z, int depths);
EXPORT_C ncnn_mat_t* ncnn_mat_depth_range(ncnn_mat_t* self, int z, int depths);
EXPORT_C ncnn_mat_t* ncnn_mat_row_range(ncnn_mat_t* self, int y, int rows);
EXPORT_C ncnn_mat_t* ncnn_mat_row_range(ncnn_mat_t* self, int y, int rows);
EXPORT_C ncnn_mat_t* ncnn_mat_range(ncnn_mat_t* self, int x, int n);
EXPORT_C ncnn_mat_t* ncnn_mat_range(ncnn_mat_t* self, int x, int n);

// === Методи класу Extractor ===
EXPORT_C void ncnn_extractor_clear(ncnn_extractor_t* self);
EXPORT_C void ncnn_extractor_set_light_mode(ncnn_extractor_t* self, bool enable);
EXPORT_C void ncnn_extractor_set_num_threads(ncnn_extractor_t* self, int num_threads);
EXPORT_C void ncnn_extractor_set_blob_allocator(ncnn_extractor_t* self, Allocator* allocator);
EXPORT_C void ncnn_extractor_set_workspace_allocator(ncnn_extractor_t* self, Allocator* allocator);
EXPORT_C void ncnn_extractor_set_vulkan_compute(ncnn_extractor_t* self, bool enable);
EXPORT_C void ncnn_extractor_set_blob_vkallocator(ncnn_extractor_t* self, VkAllocator* allocator);
EXPORT_C void ncnn_extractor_set_workspace_vkallocator(ncnn_extractor_t* self, VkAllocator* allocator);
EXPORT_C void ncnn_extractor_set_staging_vkallocator(ncnn_extractor_t* self, VkAllocator* allocator);
EXPORT_C int ncnn_extractor_input(ncnn_extractor_t* self, const char* blob_name, const Mat& in);
EXPORT_C int ncnn_extractor_extract(ncnn_extractor_t* self, const char* blob_name, Mat& feat, int type = 0);
EXPORT_C int ncnn_extractor_input(ncnn_extractor_t* self, int blob_index, const Mat& in);
EXPORT_C int ncnn_extractor_extract(ncnn_extractor_t* self, int blob_index, Mat& feat, int type = 0);
EXPORT_C int ncnn_extractor_input(ncnn_extractor_t* self, const char* blob_name, const VkMat& in);
EXPORT_C int ncnn_extractor_extract(ncnn_extractor_t* self, const char* blob_name, VkMat& feat, VkCompute& cmd);
EXPORT_C int ncnn_extractor_input(ncnn_extractor_t* self, const char* blob_name, const VkImageMat& in);
EXPORT_C int ncnn_extractor_extract(ncnn_extractor_t* self, const char* blob_name, VkImageMat& feat, VkCompute& cmd);
EXPORT_C int ncnn_extractor_input(ncnn_extractor_t* self, int blob_index, const VkMat& in);
EXPORT_C int ncnn_extractor_extract(ncnn_extractor_t* self, int blob_index, VkMat& feat, VkCompute& cmd);
EXPORT_C int ncnn_extractor_input(ncnn_extractor_t* self, int blob_index, const VkImageMat& in);
EXPORT_C int ncnn_extractor_extract(ncnn_extractor_t* self, int blob_index, VkImageMat& feat, VkCompute& cmd);

// === Методи класу Command ===

#ifdef __cplusplus
}
#endif

#endif // NCNN_C_WRAPPER_H
