// АВТОГЕНЕРАЦІЯ ЧЕРЕЗ PYTHON. НЕ РЕДАГУВАТИ!
#include "ncnn_c_wrapper.h"
#include "net.h"
#include "mat.h"
#include "command.h"

extern "C" {

// Реалізація базового створення/видалення
ncnn_net_t* ncnn_net_create() { return reinterpret_cast<ncnn_net_t*>(new ncnn::Net()); }
void ncnn_net_destroy(ncnn_net_t* net) { delete reinterpret_cast<ncnn::Net*>(net); }

ncnn_mat_t* ncnn_mat_create() { return reinterpret_cast<ncnn_mat_t*>(new ncnn::Mat()); }
void ncnn_mat_destroy(ncnn_mat_t* mat) { delete reinterpret_cast<ncnn::Mat*>(mat); }

ncnn_extractor_t* ncnn_extractor_create(ncnn_net_t* net) {
    ncnn::Net* cpp_net = reinterpret_cast<ncnn::Net*>(net);
    return reinterpret_cast<ncnn_extractor_t*>(new ncnn::Extractor(cpp_net->create_extractor()));
}
void ncnn_extractor_destroy(ncnn_extractor_t* ex) { delete reinterpret_cast<ncnn::Extractor*>(ex); }


// === Реалізація для Net ===
void ncnn_net_set_vulkan_device(ncnn_net_t* self, int device_index) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    obj->set_vulkan_device(device_index);
}

void ncnn_net_set_vulkan_device(ncnn_net_t* self, const VulkanDevice* vkdev) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    obj->set_vulkan_device(vkdev);
}

VulkanDevice* ncnn_net_vulkan_device(ncnn_net_t* self) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->vulkan_device();
}

int ncnn_net_register_custom_layer(ncnn_net_t* self, const char* type, layer_creator_func creator, layer_destroyer_func destroyer = 0, void* userdata = 0) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->register_custom_layer(type, creator, destroyer, userdata);
}

int ncnn_net_custom_layer_to_index(ncnn_net_t* self, const char* type) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->custom_layer_to_index(type);
}

int ncnn_net_register_custom_layer(ncnn_net_t* self, int index, layer_creator_func creator, layer_destroyer_func destroyer = 0, void* userdata = 0) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->register_custom_layer(index, creator, destroyer, userdata);
}

int ncnn_net_load_param(ncnn_net_t* self, const DataReader& dr) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->load_param(dr);
}

int ncnn_net_load_param_bin(ncnn_net_t* self, const DataReader& dr) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->load_param_bin(dr);
}

int ncnn_net_load_model(ncnn_net_t* self, const DataReader& dr) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->load_model(dr);
}

int ncnn_net_load_param(ncnn_net_t* self, FILE* fp) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->load_param(fp);
}

int ncnn_net_load_param(ncnn_net_t* self, const char* protopath) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->load_param(protopath);
}

int ncnn_net_load_param_mem(ncnn_net_t* self, const char* mem) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->load_param_mem(mem);
}

int ncnn_net_load_param_bin(ncnn_net_t* self, FILE* fp) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->load_param_bin(fp);
}

int ncnn_net_load_param_bin(ncnn_net_t* self, const char* protopath) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->load_param_bin(protopath);
}

int ncnn_net_load_model(ncnn_net_t* self, FILE* fp) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->load_model(fp);
}

int ncnn_net_load_model(ncnn_net_t* self, const char* modelpath) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->load_model(modelpath);
}

int ncnn_net_load_param(ncnn_net_t* self, const unsigned char* mem) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->load_param(mem);
}

int ncnn_net_load_model(ncnn_net_t* self, const unsigned char* mem) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->load_model(mem);
}

int ncnn_net_load_param(ncnn_net_t* self, AAsset* asset) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->load_param(asset);
}

int ncnn_net_load_param(ncnn_net_t* self, AAssetManager* mgr, const char* assetpath) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->load_param(mgr, assetpath);
}

int ncnn_net_load_param_bin(ncnn_net_t* self, AAsset* asset) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->load_param_bin(asset);
}

int ncnn_net_load_param_bin(ncnn_net_t* self, AAssetManager* mgr, const char* assetpath) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->load_param_bin(mgr, assetpath);
}

int ncnn_net_load_model(ncnn_net_t* self, AAsset* asset) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->load_model(asset);
}

int ncnn_net_load_model(ncnn_net_t* self, AAssetManager* mgr, const char* assetpath) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->load_model(mgr, assetpath);
}

void ncnn_net_clear(ncnn_net_t* self) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    obj->clear();
}

Extractor ncnn_net_create_extractor(ncnn_net_t* self) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    auto res = obj->create_extractor();
    return reinterpret_cast<Extractor>(new ncnn::Extractor(res));
}

std::vector<int>& ncnn_net_input_indexes(ncnn_net_t* self) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->input_indexes();
}

std::vector<int>& ncnn_net_output_indexes(ncnn_net_t* self) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->output_indexes();
}

char*>& ncnn_net_input_names(ncnn_net_t* self) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->input_names();
}

char*>& ncnn_net_output_names(ncnn_net_t* self) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->output_names();
}

std::vector<Blob>& ncnn_net_blobs(ncnn_net_t* self) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->blobs();
}

std::vector<Layer*>& ncnn_net_layers(ncnn_net_t* self) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->layers();
}

std::vector<Blob>& ncnn_net_mutable_blobs(ncnn_net_t* self) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->mutable_blobs();
}

std::vector<Layer*>& ncnn_net_mutable_layers(ncnn_net_t* self) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->mutable_layers();
}

int ncnn_net_find_blob_index_by_name(ncnn_net_t* self, const char* name) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->find_blob_index_by_name(name);
}

int ncnn_net_find_layer_index_by_name(ncnn_net_t* self, const char* name) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->find_layer_index_by_name(name);
}

Layer* ncnn_net_create_custom_layer(ncnn_net_t* self, const char* type) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->create_custom_layer(type);
}

Layer* ncnn_net_create_custom_layer(ncnn_net_t* self, int index) {
    auto* obj = reinterpret_cast<ncnn::Net*>(self);
    return obj->create_custom_layer(index);
}


// === Реалізація для Mat ===
void ncnn_mat_fill(ncnn_mat_t* self, float v) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->fill(v);
}

void ncnn_mat_fill(ncnn_mat_t* self, int v) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->fill(v);
}

void ncnn_mat_fill(ncnn_mat_t* self, float32x4_t _v) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->fill(_v);
}

void ncnn_mat_fill(ncnn_mat_t* self, uint16x4_t _v) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->fill(_v);
}

void ncnn_mat_fill(ncnn_mat_t* self, int32x4_t _v) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->fill(_v);
}

void ncnn_mat_fill(ncnn_mat_t* self, int32x4_t _v0, int32x4_t _v1) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->fill(_v0, _v1);
}

void ncnn_mat_fill(ncnn_mat_t* self, float16x4_t _v) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->fill(_v);
}

void ncnn_mat_fill(ncnn_mat_t* self, float16x8_t _v) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->fill(_v);
}

void ncnn_mat_fill(ncnn_mat_t* self, __m512 _v) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->fill(_v);
}

void ncnn_mat_fill(ncnn_mat_t* self, __m256 _v, int i = 0) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->fill(_v, i);
}

void ncnn_mat_fill(ncnn_mat_t* self, __m128 _v) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->fill(_v);
}

void ncnn_mat_fill(ncnn_mat_t* self, __m128i _v) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->fill(_v);
}

void ncnn_mat_fill(ncnn_mat_t* self, v4f32 _v) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->fill(_v);
}

void ncnn_mat_fill(ncnn_mat_t* self, vfloat32m1_t _v) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->fill(_v);
}

void ncnn_mat_fill(ncnn_mat_t* self, vuint16m1_t _v) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->fill(_v);
}

void ncnn_mat_fill(ncnn_mat_t* self, vint8m1_t _v) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->fill(_v);
}

void ncnn_mat_fill(ncnn_mat_t* self, vfloat16m1_t _v) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->fill(_v);
}

void ncnn_mat_fill(ncnn_mat_t* self, T v) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->fill(v);
}

ncnn_mat_t* ncnn_mat_clone(ncnn_mat_t* self, Allocator* allocator = 0) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->clone(allocator);
}

void ncnn_mat_clone_from(ncnn_mat_t* self, const ncnn::Mat& mat, Allocator* allocator = 0) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->clone_from(mat, allocator);
}

ncnn_mat_t* ncnn_mat_reshape(ncnn_mat_t* self, int w, Allocator* allocator = 0) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->reshape(w, allocator);
}

ncnn_mat_t* ncnn_mat_reshape(ncnn_mat_t* self, int w, int h, Allocator* allocator = 0) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->reshape(w, h, allocator);
}

ncnn_mat_t* ncnn_mat_reshape(ncnn_mat_t* self, int w, int h, int c, Allocator* allocator = 0) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->reshape(w, h, c, allocator);
}

ncnn_mat_t* ncnn_mat_reshape(ncnn_mat_t* self, int w, int h, int d, int c, Allocator* allocator = 0) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->reshape(w, h, d, c, allocator);
}

void ncnn_mat_create(ncnn_mat_t* self, int w, size_t elemsize = 4u, Allocator* allocator = 0) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->create(w, elemsize, allocator);
}

void ncnn_mat_create(ncnn_mat_t* self, int w, int h, size_t elemsize = 4u, Allocator* allocator = 0) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->create(w, h, elemsize, allocator);
}

void ncnn_mat_create(ncnn_mat_t* self, int w, int h, int c, size_t elemsize = 4u, Allocator* allocator = 0) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->create(w, h, c, elemsize, allocator);
}

void ncnn_mat_create(ncnn_mat_t* self, int w, int h, int d, int c, size_t elemsize = 4u, Allocator* allocator = 0) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->create(w, h, d, c, elemsize, allocator);
}

void ncnn_mat_create(ncnn_mat_t* self, int w, size_t elemsize, int elempack, Allocator* allocator = 0) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->create(w, elemsize, elempack, allocator);
}

void ncnn_mat_create(ncnn_mat_t* self, int w, int h, size_t elemsize, int elempack, Allocator* allocator = 0) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->create(w, h, elemsize, elempack, allocator);
}

void ncnn_mat_create(ncnn_mat_t* self, int w, int h, int c, size_t elemsize, int elempack, Allocator* allocator = 0) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->create(w, h, c, elemsize, elempack, allocator);
}

void ncnn_mat_create(ncnn_mat_t* self, int w, int h, int d, int c, size_t elemsize, int elempack, Allocator* allocator = 0) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->create(w, h, d, c, elemsize, elempack, allocator);
}

void ncnn_mat_create_like(ncnn_mat_t* self, const Mat& m, Allocator* allocator = 0) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->create_like(m, allocator);
}

void ncnn_mat_create_like(ncnn_mat_t* self, const VkMat& m, Allocator* allocator = 0) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->create_like(m, allocator);
}

void ncnn_mat_create_like(ncnn_mat_t* self, const VkImageMat& im, Allocator* allocator = 0) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->create_like(im, allocator);
}

void ncnn_mat_addref(ncnn_mat_t* self) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->addref();
}

void ncnn_mat_release(ncnn_mat_t* self) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    obj->release();
}

bool ncnn_mat_empty(ncnn_mat_t* self) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->empty();
}

size_t ncnn_mat_total(ncnn_mat_t* self) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->total();
}

int ncnn_mat_elembits(ncnn_mat_t* self) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->elembits();
}

ncnn_mat_t* ncnn_mat_shape(ncnn_mat_t* self) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->shape();
}

ncnn_mat_t* ncnn_mat_channel(ncnn_mat_t* self, int c) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->channel(c);
}

ncnn_mat_t* ncnn_mat_channel(ncnn_mat_t* self, int c) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->channel(c);
}

ncnn_mat_t* ncnn_mat_depth(ncnn_mat_t* self, int z) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->depth(z);
}

ncnn_mat_t* ncnn_mat_depth(ncnn_mat_t* self, int z) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->depth(z);
}

float* ncnn_mat_row(ncnn_mat_t* self, int y) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->row(y);
}

float* ncnn_mat_row(ncnn_mat_t* self, int y) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->row(y);
}

T* ncnn_mat_row(ncnn_mat_t* self, int y) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->row(y);
}

T* ncnn_mat_row(ncnn_mat_t* self, int y) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->row(y);
}

ncnn_mat_t* ncnn_mat_channel_range(ncnn_mat_t* self, int c, int channels) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->channel_range(c, channels);
}

ncnn_mat_t* ncnn_mat_channel_range(ncnn_mat_t* self, int c, int channels) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->channel_range(c, channels);
}

ncnn_mat_t* ncnn_mat_depth_range(ncnn_mat_t* self, int z, int depths) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->depth_range(z, depths);
}

ncnn_mat_t* ncnn_mat_depth_range(ncnn_mat_t* self, int z, int depths) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->depth_range(z, depths);
}

ncnn_mat_t* ncnn_mat_row_range(ncnn_mat_t* self, int y, int rows) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->row_range(y, rows);
}

ncnn_mat_t* ncnn_mat_row_range(ncnn_mat_t* self, int y, int rows) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->row_range(y, rows);
}

ncnn_mat_t* ncnn_mat_range(ncnn_mat_t* self, int x, int n) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->range(x, n);
}

ncnn_mat_t* ncnn_mat_range(ncnn_mat_t* self, int x, int n) {
    auto* obj = reinterpret_cast<ncnn::Mat*>(self);
    return obj->range(x, n);
}


// === Реалізація для Extractor ===
void ncnn_extractor_clear(ncnn_extractor_t* self) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    obj->clear();
}

void ncnn_extractor_set_light_mode(ncnn_extractor_t* self, bool enable) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    obj->set_light_mode(enable);
}

void ncnn_extractor_set_num_threads(ncnn_extractor_t* self, int num_threads) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    obj->set_num_threads(num_threads);
}

void ncnn_extractor_set_blob_allocator(ncnn_extractor_t* self, Allocator* allocator) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    obj->set_blob_allocator(allocator);
}

void ncnn_extractor_set_workspace_allocator(ncnn_extractor_t* self, Allocator* allocator) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    obj->set_workspace_allocator(allocator);
}

void ncnn_extractor_set_vulkan_compute(ncnn_extractor_t* self, bool enable) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    obj->set_vulkan_compute(enable);
}

void ncnn_extractor_set_blob_vkallocator(ncnn_extractor_t* self, VkAllocator* allocator) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    obj->set_blob_vkallocator(allocator);
}

void ncnn_extractor_set_workspace_vkallocator(ncnn_extractor_t* self, VkAllocator* allocator) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    obj->set_workspace_vkallocator(allocator);
}

void ncnn_extractor_set_staging_vkallocator(ncnn_extractor_t* self, VkAllocator* allocator) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    obj->set_staging_vkallocator(allocator);
}

int ncnn_extractor_input(ncnn_extractor_t* self, const char* blob_name, const Mat& in) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    return obj->input(blob_name, in);
}

int ncnn_extractor_extract(ncnn_extractor_t* self, const char* blob_name, Mat& feat, int type = 0) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    return obj->extract(blob_name, feat, type);
}

int ncnn_extractor_input(ncnn_extractor_t* self, int blob_index, const Mat& in) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    return obj->input(blob_index, in);
}

int ncnn_extractor_extract(ncnn_extractor_t* self, int blob_index, Mat& feat, int type = 0) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    return obj->extract(blob_index, feat, type);
}

int ncnn_extractor_input(ncnn_extractor_t* self, const char* blob_name, const VkMat& in) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    return obj->input(blob_name, in);
}

int ncnn_extractor_extract(ncnn_extractor_t* self, const char* blob_name, VkMat& feat, VkCompute& cmd) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    return obj->extract(blob_name, feat, cmd);
}

int ncnn_extractor_input(ncnn_extractor_t* self, const char* blob_name, const VkImageMat& in) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    return obj->input(blob_name, in);
}

int ncnn_extractor_extract(ncnn_extractor_t* self, const char* blob_name, VkImageMat& feat, VkCompute& cmd) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    return obj->extract(blob_name, feat, cmd);
}

int ncnn_extractor_input(ncnn_extractor_t* self, int blob_index, const VkMat& in) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    return obj->input(blob_index, in);
}

int ncnn_extractor_extract(ncnn_extractor_t* self, int blob_index, VkMat& feat, VkCompute& cmd) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    return obj->extract(blob_index, feat, cmd);
}

int ncnn_extractor_input(ncnn_extractor_t* self, int blob_index, const VkImageMat& in) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    return obj->input(blob_index, in);
}

int ncnn_extractor_extract(ncnn_extractor_t* self, int blob_index, VkImageMat& feat, VkCompute& cmd) {
    auto* obj = reinterpret_cast<ncnn::Extractor*>(self);
    return obj->extract(blob_index, feat, cmd);
}


// === Реалізація для Command ===
}
