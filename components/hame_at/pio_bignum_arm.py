# PlatformIO pre-script (registered by hame_at's __init__.py), not an
# ESPHome module.
#
# The BK72xx SDK is built in Thumb-1 (-mthumb, ARMv5TE), where a 32x32->64
# multiply does not exist: mbedTLS's bn_mul.h then multiplies in 16-bit
# halves, and the RSA-2048 signature of a TLS handshake with a client
# certificate (Hame's AWS endpoint) takes ~10 s. In ARM mode bn_mul.h uses
# UMLAL. Build just mbedTLS's bignum.c in ARM mode; interworking with the
# Thumb code is the AAPCS default (the SDK also passes -mthumb-interwork).

Import("env")  # noqa: F821 (SCons)


def _arm_mode(env, node):
    path = node.srcnode().get_path()
    if "mbedtls" not in path.replace("\\", "/"):
        return node
    return env.Object(node, CCFLAGS=env["CCFLAGS"] + ["-marm"])


env.AddBuildMiddleware(_arm_mode, "*bignum.c")  # noqa: F821
