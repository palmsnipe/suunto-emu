#include "semu/hash.h"
#include "semu/manifest.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static const char RESIDENT_SHA[] =
    "c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5";
static const char APP_SHA[] =
    "85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89";
static const char RES_SHA[] =
    "49a3936f4c9d61dbceee12324f41412334c42aa98b900f6f2d3fc5633ae43aea";

static void test_profile_loads(semu_test_context *context)
{
    semu_profile profile;
    semu_error error;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_profile_load("profiles/sapporo/2.39.20/profile.semu",
                          &profile, &error));
    SEMU_TEST_ASSERT(context, strcmp(profile.id, "sapporo-2.39.20") == 0);
    SEMU_TEST_ASSERT(context, strcmp(profile.board, "sapporo") == 0);
    SEMU_TEST_ASSERT(context, strcmp(profile.product, "Sapporo") == 0);
    SEMU_TEST_ASSERT(context,
        strcmp(profile.version, "2.39.20.22297-P") == 0);
    SEMU_TEST_EQ_U64(context, 0x00040000u, profile.vector_table);
    SEMU_TEST_EQ_U64(context, 240u, profile.display_width);
    SEMU_TEST_EQ_U64(context, 240u, profile.display_height);
    SEMU_TEST_EQ_U64(context, 3u, profile.required_count);
    SEMU_TEST_EQ_U64(context, 2u, profile.layer_count);
    SEMU_TEST_ASSERT(context,
        strcmp(profile.layers[1], "sapporo-2.39-gps-startup") == 0);
    SEMU_TEST_ASSERT(context,
        strcmp(profile.layers[0], "sapporo-2.39-synthetic-wbsto") == 0);
}

static void test_profile_components(semu_test_context *context)
{
    semu_profile profile;
    semu_error error;
    uint8_t expected[SEMU_SHA256_SIZE];
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_profile_load("profiles/sapporo/2.39.20/profile.semu",
                          &profile, &error));
    SEMU_TEST_ASSERT(context,
        strcmp(profile.required[0].role, "resident") == 0);
    SEMU_TEST_EQ_U64(context, 0x00019000u,
        profile.required[0].load_address);
    SEMU_TEST_EQ_U64(context, 73377u, profile.required[0].size);
    SEMU_TEST_ASSERT(context, semu_sha256_parse(RESIDENT_SHA, expected) == 1);
    SEMU_TEST_ASSERT(context, memcmp(profile.required[0].sha256, expected,
        SEMU_SHA256_SIZE) == 0);
    SEMU_TEST_ASSERT(context,
        strcmp(profile.required[1].role, "application") == 0);
    SEMU_TEST_EQ_U64(context, 0x00040000u,
        profile.required[1].load_address);
    SEMU_TEST_EQ_U64(context, 1596337u, profile.required[1].size);
    SEMU_TEST_ASSERT(context, semu_sha256_parse(APP_SHA, expected) == 1);
    SEMU_TEST_ASSERT(context, memcmp(profile.required[1].sha256, expected,
        SEMU_SHA256_SIZE) == 0);
    SEMU_TEST_ASSERT(context,
        strcmp(profile.required[2].role, "resources") == 0);
    SEMU_TEST_EQ_U64(context, 0x14000000u,
        profile.required[2].load_address);
    SEMU_TEST_EQ_U64(context, 16519168u, profile.required[2].size);
    SEMU_TEST_ASSERT(context, semu_sha256_parse(RES_SHA, expected) == 1);
    SEMU_TEST_ASSERT(context, memcmp(profile.required[2].sha256, expected,
        SEMU_SHA256_SIZE) == 0);
}

static int write_text(const char *path, const char *text)
{
    FILE *stream = fopen(path, "wb");
    size_t length = strlen(text);
    if (stream == NULL) return 0;
    if (fwrite(text, 1u, length, stream) != length) {
        (void)fclose(stream);
        return 0;
    }
    return fclose(stream) == 0;
}

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash == NULL ? path : slash + 1;
}

static int make_contract(char profile_path[128], char manifest_path[128],
                         char component_path[128], const char *version)
{
    uint8_t digest[SEMU_SHA256_SIZE];
    char hash[65];
    char profile_buf[1024];
    char manifest_buf[1024];
    if (!semu_test_temp_path(profile_path, 128u, "profile.semu") ||
        !semu_test_temp_path(manifest_path, 128u, "firmware.semu") ||
        !semu_test_temp_path(component_path, 128u, "app.bin")) return 0;
    if (!write_text(component_path, "abc")) return 0;
    semu_sha256("abc", 3u, digest);
    semu_sha256_format(digest, hash);
    (void)snprintf(profile_buf, sizeof(profile_buf),
        "[profile]\nformat=1\nid=sapporo-2.39.20\nboard=sapporo\n"
        "product=Sapporo\nversion=%s\nvector_table=0x00040000\n"
        "display_width=240\ndisplay_height=240\n\n"
        "[component app]\nrole=application\nload_address=0x00040000\n"
        "size=3\nsha256=%s\n", version, hash);
    (void)snprintf(manifest_buf, sizeof(manifest_buf),
        "[firmware]\nformat=1\nproduct=Sapporo\nversion=%s\n\n"
        "[component app]\nrole=application\npath=%s\nload_address=0x00040000\n"
        "size=3\nsha256=%s\n", version, base_name(component_path), hash);
    return write_text(profile_path, profile_buf) &&
           write_text(manifest_path, manifest_buf);
}

static void test_valid_manifest(semu_test_context *context)
{
    char profile_path[128], manifest_path[128], component_path[128];
    semu_profile profile;
    semu_firmware_manifest manifest;
    semu_error error;
    SEMU_TEST_ASSERT(context,
        make_contract(profile_path, manifest_path, component_path,
                      "2.39.20.22297-P"));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_profile_load(profile_path, &profile, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_manifest_load(manifest_path, &manifest, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_manifest_validate(&profile, &manifest, &error));
    (void)remove(profile_path);
    (void)remove(manifest_path);
    (void)remove(component_path);
}

static void test_wrong_hash_rejected(semu_test_context *context)
{
    char profile_path[128], manifest_path[128], component_path[128];
    semu_profile profile;
    semu_firmware_manifest manifest;
    semu_error error;
    SEMU_TEST_ASSERT(context,
        make_contract(profile_path, manifest_path, component_path,
                      "2.39.20.22297-P"));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_profile_load(profile_path, &profile, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_manifest_load(manifest_path, &manifest, &error));
    SEMU_TEST_ASSERT(context, write_text(component_path, "abd"));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
        semu_manifest_validate(&profile, &manifest, &error));
    (void)remove(profile_path);
    (void)remove(manifest_path);
    (void)remove(component_path);
}

static void test_wrong_version_rejected(semu_test_context *context)
{
    char profile_path[128], manifest_path[128], component_path[128];
    semu_profile profile;
    semu_firmware_manifest manifest;
    semu_error error;
    SEMU_TEST_ASSERT(context,
        make_contract(profile_path, manifest_path, component_path,
                      "2.39.20.22297-P"));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_profile_load(profile_path, &profile, &error));
    SEMU_TEST_ASSERT(context,
        make_contract(profile_path, manifest_path, component_path,
                      "9.99.99.99999-P"));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_manifest_load(manifest_path, &manifest, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
        semu_manifest_validate(&profile, &manifest, &error));
    (void)remove(profile_path);
    (void)remove(manifest_path);
    (void)remove(component_path);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_profile_loads),
        SEMU_TEST_CASE(test_profile_components),
        SEMU_TEST_CASE(test_valid_manifest),
        SEMU_TEST_CASE(test_wrong_hash_rejected),
        SEMU_TEST_CASE(test_wrong_version_rejected)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
