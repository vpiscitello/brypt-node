        'conditions': [
            [ 'OS=="win"', {
                'conditions': [
                    ['target_arch=="x64"', {
                        'variables': {
                        'openssl_root%': 'C:/OpenSSL-Win64/openssl-1.1.0f-vs2017'
                    },
                    }, {
                        'variables': {
                            'openssl_root%': 'C:/OpenSSL-Win32'
                        },
                    }],
                ],
                'defines': [
                    'uint=unsigned int',
                ],
                'libraries': [
                    '-l<(openssl_root)/lib64/libcryptoMT.lib',
                    '-l<(openssl_root)/lib64/libsslMT.lib',
                ],
                'include_dirs': [
                    "<!(node -e \"require('nan')\")",
                    "<(openssl_root)/include64/openssl",
                    "<(openssl_root)/include64",
                ],
            }, { # OS!="win"
                'libraries': [
                    "-L/usr/local/opt/openssl/lib",
                    "-L/usr/local/opt/openssl/include/openssl",
                    "/usr/local/opt/openssl/lib/libcrypto.a",
                    "-lcrypto",
                    # "-lssl"
                ],
                'include_dirs': [
                    "<!(node -e \"require('nan')\")",
                    "/usr/local/opt/openssl/include/openssl",
                    "/usr/local/Cellar/openssl/1.0.2q/include/openssl",
                ],
            }]
