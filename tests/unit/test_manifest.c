#include "semu/hash.h"
#include "semu/manifest.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static int write_text(const char *path, const char *text)
{
    FILE *stream = fopen(path, "wb");
    size_t length = strlen(text);
    if (stream == NULL) return 0;
    if (fwrite(text, 1u, length, stream) != length) {
        (void)fclose(stream); return 0;
    }
    return fclose(stream) == 0;
}

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash == NULL ? path : slash + 1;
}

static int make_contract_files(char profile_path[128], char manifest_path[128],
                               char component_path[128])
{
    uint8_t digest[SEMU_SHA256_SIZE];
    char hash[65];
    char profile[1024];
    char manifest[1024];
    if (!semu_test_temp_path(profile_path, 128u, "profile.semu") ||
        !semu_test_temp_path(manifest_path, 128u, "firmware.semu") ||
        !semu_test_temp_path(component_path, 128u, "app.bin")) return 0;
    if (!write_text(component_path, "abc")) return 0;
    semu_sha256("abc", 3u, digest);
    semu_sha256_format(digest, hash);
    (void)snprintf(profile, sizeof(profile),
        "[profile]\nformat=1\nid=sapporo-2.22\nboard=sapporo\n"
        "product=sapporo\nversion=2.22.60.3383-P\nvector_table=0x0\n"
        "display_width=240\ndisplay_height=240\n\n"
        "[component app]\nrole=application\nload_address=0x10000\n"
        "size=3\nsha256=%s\n\n[layer no-device]\nkind=fixture\n"
        "evidence=synthetic-test\nmax_hits=2\n", hash);
    (void)snprintf(manifest, sizeof(manifest),
        "[firmware]\nformat=1\nproduct=sapporo\nversion=2.22.60.3383-P\n\n"
        "[component app]\nrole=application\npath=%s\nload_address=0x10000\n"
        "size=3\nsha256=%s\n", base_name(component_path), hash);
    return write_text(profile_path, profile) && write_text(manifest_path, manifest);
}

static void remove_contract_files(const char *profile, const char *manifest,
                                  const char *component)
{
    (void)remove(profile); (void)remove(manifest); (void)remove(component);
}

static void test_load_and_validate(semu_test_context *context)
{
    char profile_path[128], manifest_path[128], component_path[128];
    semu_profile profile;
    semu_firmware_manifest manifest;
    semu_error error;
    SEMU_TEST_ASSERT(context,
        make_contract_files(profile_path, manifest_path, component_path));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_profile_load(profile_path, &profile, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_manifest_load(manifest_path, &manifest, &error));
    SEMU_TEST_EQ_U64(context, 1u, profile.required_count);
    SEMU_TEST_EQ_U64(context, 1u, profile.layer_count);
    SEMU_TEST_ASSERT(context, strcmp(profile.layers[0], "no-device") == 0);
    SEMU_TEST_ASSERT(context, strcmp(manifest.components[0].path,
                                    component_path) == 0);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_manifest_validate(&profile, &manifest, &error));
    SEMU_TEST_ASSERT(context, write_text(component_path, "abd"));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
                     semu_manifest_validate(&profile, &manifest, &error));
    remove_contract_files(profile_path, manifest_path, component_path);
}

static void test_rejects_unsafe_path(semu_test_context *context)
{
    char path[128];
    semu_firmware_manifest manifest;
    semu_error error;
    static const char text[] =
        "[firmware]\nformat=1\nproduct=sapporo\nversion=v\n"
        "[component app]\nrole=application\npath=../app.bin\n"
        "load_address=0\nsize=1\n"
        "sha256=0000000000000000000000000000000000000000000000000000000000000000\n";
    SEMU_TEST_ASSERT(context, semu_test_temp_path(path, sizeof(path), "unsafe.semu"));
    SEMU_TEST_ASSERT(context, write_text(path, text));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_manifest_load(path, &manifest, &error));
    SEMU_TEST_ASSERT(context, strstr(error.text, "unsafe component path") != NULL);
    (void)remove(path);
}

static void test_rejects_unknown_and_duplicate_keys(semu_test_context *context)
{
    char path[128];
    semu_profile profile;
    semu_error error;
    static const char text[] =
        "[profile]\nformat=1\nformat=1\nid=x\nboard=x\nproduct=x\nversion=x\n"
        "vector_table=0\ndisplay_width=1\ndisplay_height=1\n";
    SEMU_TEST_ASSERT(context, semu_test_temp_path(path, sizeof(path), "duplicate.semu"));
    SEMU_TEST_ASSERT(context, write_text(path, text));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_profile_load(path, &profile, &error));
    SEMU_TEST_ASSERT(context, strstr(error.text, "duplicate key") != NULL);
    (void)remove(path);
}

static void test_rejects_unknown_key(semu_test_context *context)
{
    char path[128];
    semu_firmware_manifest manifest;
    semu_error error;
    static const char text[] =
        "[firmware]\nformat=1\nproduct=x\nversion=x\nsurprise=value\n";
    SEMU_TEST_ASSERT(context, semu_test_temp_path(path, sizeof(path), "unknown.semu"));
    SEMU_TEST_ASSERT(context, write_text(path, text));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_manifest_load(path, &manifest, &error));
    SEMU_TEST_ASSERT(context, strstr(error.text, "unknown root key") != NULL);
    (void)remove(path);
}

static void test_rejects_profile_component_path(semu_test_context *context)
{
    char path[128];
    semu_profile profile;
    semu_error error;
    static const char text[] =
        "[profile]\nformat=1\nid=x\nboard=x\nproduct=x\nversion=x\n"
        "vector_table=0\ndisplay_width=1\ndisplay_height=1\n"
        "[component app]\nrole=application\npath=app.bin\nload_address=0\nsize=1\n"
        "sha256=0000000000000000000000000000000000000000000000000000000000000000\n";
    SEMU_TEST_ASSERT(context, semu_test_temp_path(path, sizeof(path), "profile-path.semu"));
    SEMU_TEST_ASSERT(context, write_text(path, text));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
                     semu_profile_load(path, &profile, &error));
    (void)remove(path);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_load_and_validate),
        SEMU_TEST_CASE(test_rejects_unsafe_path),
        SEMU_TEST_CASE(test_rejects_unknown_and_duplicate_keys),
        SEMU_TEST_CASE(test_rejects_unknown_key),
        SEMU_TEST_CASE(test_rejects_profile_component_path)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
