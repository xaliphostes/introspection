{
  "targets": [
    {
      "target_name": "introspection_demo",  # Creates introspection_demo.node
      "sources": [
        "binding.cxx"                       # Our main C++ file
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",  # N-API headers
        "../../include"                                        # Our introspection headers
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"       # N-API dependency
      ],
      "cflags!": [ "-fno-exceptions" ],      # Remove no-exceptions flag
      "cflags_cc!": [ "-fno-exceptions" ],   # Remove no-exceptions flag (C++)
      "xcode_settings": {                    # macOS-specific settings
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "CLANG_CXX_LIBRARY": "libc++",
        "MACOSX_DEPLOYMENT_TARGET": "10.7"
      },
      "msvs_settings": {                     # Windows Visual Studio settings
        "VCCLCompilerTool": { 
          "ExceptionHandling": 1 
        }
      },
      "cflags_cc": [
        "-std=c++20",                        # Use C++20 standard
        "-fexceptions"                       # Enable C++ exceptions
      ],
      "conditions": [                        # Conditional compilation
        ["OS==\"win\"", {
          "defines": [
            "_HAS_EXCEPTIONS=1"              # Windows-specific exception define
          ]
        }]
      ]
    }
  ]
}