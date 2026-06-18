import os

def patch_c_api(file_path):
    if not os.path.exists(file_path):
        print(f"[ERROR] Файл {file_path} не знайдено! Перевір шлях.")
        return

    with open(file_path, "r", encoding="utf-8") as f:
        code = f.read()

    print("[*] Патчимо c_api.cpp для обходу конфліктів конфігурації...")

    # 1. Фікс для ncnn_layer_get_support_image_storage
    # Заміняємо повернення неіснуючого члена на return 0;
    old_layer_get = "return ((const Layer*)layer->pthis)->support_image_storage;"
    new_layer_get = "// return ((const Layer*)layer->pthis)->support_image_storage;\n    return 0;"
    code = code.replace(old_layer_get, new_layer_get)

    # 2. Фікс для ncnn_layer_set_support_image_storage
    old_layer_set = "((Layer*)layer->pthis)->support_image_storage = enable;"
    new_layer_set = "// ((Layer*)layer->pthis)->support_image_storage = enable;"
    code = code.replace(old_layer_set, new_layer_set)

    # 3. Фікс всередині __Layer_c_api_layer_creator
    old_creator_assign = "layer->support_image_storage = ncnn_layer_get_support_image_storage(layer0);"
    new_creator_assign = "// layer->support_image_storage = ncnn_layer_get_support_image_storage(layer0);"
    code = code.replace(old_creator_assign, new_creator_assign)

    # 4. Фікс для ncnn_extractor_set_option (set_num_threads та set_vulkan_compute)
    old_extractor_threads = "((Extractor*)ex)->set_num_threads(((const Option*)opt)->num_threads);"
    new_extractor_threads = "// ((Extractor*)ex)->set_num_threads(((const Option*)opt)->num_threads);"
    code = code.replace(old_extractor_threads, new_extractor_threads)

    old_extractor_vulkan = "((Extractor*)ex)->set_vulkan_compute(((const Option*)opt)->use_vulkan_compute);"
    new_extractor_vulkan = "// ((Extractor*)ex)->set_vulkan_compute(((const Option*)opt)->use_vulkan_compute);"
    code = code.replace(old_extractor_vulkan, new_extractor_vulkan)

    # Записуємо виправлений код назад
    with open(file_path, "w", encoding="utf-8") as f:
        f.write(code)

    print("[SUCCESS] c_api.cpp успішно пропатчено! Можна запускати компіляцію.")

if __name__ == "__main__":
    # Якщо c_api.cpp лежить в іншій папці, просто зміни шлях тут
    target_file = "c_api.cpp" 
    patch_c_api(target_file)