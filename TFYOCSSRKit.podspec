Pod::Spec.new do |spec|


  spec.name         = "TFYOCSSRKit"

  spec.version      = "1.0.0"

  spec.summary      = "iOS SSR工具库"


  spec.description  = <<-DESC
                    这是一个iOS SSR工具库，提供了SSR相关的功能实现
                    DESC

  spec.homepage     = "https://github.com/13662049573/TFYOCSSRUilt"
 

  spec.license      = { :type => "MIT", :file => "LICENSE" }


  spec.author       = { "田风有" => "420144542@qq.com" }
  

  spec.platform     = :ios, "15.0"

 
  spec.source       = { :git => "https://github.com/13662049573/TFYOCSSRUilt.git", :tag => spec.version }
  

  spec.source_files = "TFYOCSSRUilt/TFYOCSSRKit/TFYOCSSRKit.h"
 
  # 添加子规范来分别包含不同模块
  spec.subspec 'Antinat' do |ss|
    ss.source_files = "TFYOCSSRKit/Antinat/**/*.{h,m,c}"
    ss.public_header_files = "TFYOCSSRKit/Antinat/**/*.h"
    ss.vendored_libraries = "TFYOCSSRKit/Antinat/expat-lib/lib/libexpat.a"
  end
  
  spec.subspec 'libmaxminddb' do |ss|
    ss.source_files = [
      "TFYOCSSRKit/libmaxminddb/include/**/*.h",
      "TFYOCSSRKit/libmaxminddb/maxminddb_config.h",
      "TFYOCSSRKit/libmaxminddb/maxminddb.h"
    ]
    
    ss.public_header_files = [
      "TFYOCSSRKit/libmaxminddb/include/**/*.h",
      "TFYOCSSRKit/libmaxminddb/maxminddb_config.h",
      "TFYOCSSRKit/libmaxminddb/maxminddb.h"
    ]
    
    # iOS 平台使用的库文件
    ss.ios.vendored_libraries = "TFYOCSSRKit/libmaxminddb/lib/libmaxminddb_ios.a"
    
    # macOS 平台使用的库文件
    ss.osx.vendored_libraries = "TFYOCSSRKit/libmaxminddb/lib/libmaxminddb_macos.a"
    
    ss.preserve_paths = [
      'TFYOCSSRKit/libmaxminddb/include/**',
      'TFYOCSSRKit/libmaxminddb/lib/**'
    ]
    
    ss.header_mappings_dir = 'TFYOCSSRKit/libmaxminddb'
    
    ss.pod_target_xcconfig = {
      'HEADER_SEARCH_PATHS' => [
        '$(inherited)',
        '"$(PODS_TARGET_SRCROOT)/TFYOCSSRKit/libmaxminddb/include"',
        '"$(PODS_TARGET_SRCROOT)/TFYOCSSRKit/libmaxminddb"'
      ].join(' ')
    }
  end
  
  spec.subspec 'libopenssl' do |ss|
    ss.source_files = [
      "TFYOCSSRKit/libopenssl/include/**/*.h",
      "TFYOCSSRKit/libopenssl/include/openssl/*.h",
      "TFYOCSSRKit/libopenssl/lib/**/*.{h,c}"
    ]
    
    ss.public_header_files = [
      "TFYOCSSRKit/libopenssl/include/**/*.h",
      "TFYOCSSRKit/libopenssl/include/openssl/*.h"
    ]
    
    # iOS 平台使用的库文件
    ss.ios.vendored_libraries = [
      "TFYOCSSRKit/libopenssl/lib/libcrypto_ios.a",
      "TFYOCSSRKit/libopenssl/lib/libssl_ios.a"
    ]
    
    # macOS 平台使用的库文件
    ss.osx.vendored_libraries = [
      "TFYOCSSRKit/libopenssl/lib/libcrypto_macos.a",
      "TFYOCSSRKit/libopenssl/lib/libssl_macos.a"
    ]
    
    ss.preserve_paths = [
      'TFYOCSSRKit/libopenssl/include/**',
      'TFYOCSSRKit/libopenssl/lib/**'
    ]
    
    ss.header_mappings_dir = 'TFYOCSSRKit/libopenssl'
    
    ss.pod_target_xcconfig = {
      'HEADER_SEARCH_PATHS' => [
        '$(inherited)',
        '"$(PODS_TARGET_SRCROOT)/TFYOCSSRKit/libopenssl/include"',
        '"$(PODS_TARGET_SRCROOT)/TFYOCSSRKit/libopenssl/include/openssl"',
        '"$(PODS_TARGET_SRCROOT)/TFYOCSSRKit/libopenssl/lib"'
      ].join(' ')
    }
  end
  
  spec.subspec 'libsodium' do |ss|
    ss.source_files = [
      "TFYOCSSRKit/libsodium/include/**/*.h",
      "TFYOCSSRKit/libsodium/include/sodium/*.h"
    ]
    
    ss.public_header_files = [
      "TFYOCSSRKit/libsodium/include/**/*.h",
      "TFYOCSSRKit/libsodium/include/sodium/*.h"
    ]
    
    # iOS 平台使用的库文件
    ss.ios.vendored_libraries = "TFYOCSSRKit/libsodium/lib/libsodium_ios.a"
    
    # macOS 平台使用的库文件
    ss.osx.vendored_libraries = "TFYOCSSRKit/libsodium/lib/libsodium_macos.a"
    
    ss.preserve_paths = [
      'TFYOCSSRKit/libsodium/include/**',
      'TFYOCSSRKit/libsodium/lib/**'
    ]
    
    ss.header_mappings_dir = 'TFYOCSSRKit/libsodium/include'
    
    ss.pod_target_xcconfig = {
      'HEADER_SEARCH_PATHS' => [
        '$(inherited)',
        '"$(PODS_TARGET_SRCROOT)/TFYOCSSRKit/libsodium/include"',
        '"$(PODS_TARGET_SRCROOT)/TFYOCSSRKit/libsodium/include/sodium"'
      ].join(' ')
    }
  end
  
  spec.subspec 'shadowsocks-libev' do |ss|
    # 移除 libcork 子规范，直接在主规范中包含 libcork 文件
    ss.source_files = [
      "TFYOCSSRKit/shadowsocks-libev/**/*.{h,m,c}",
      "TFYOCSSRKit/shadowsocks-libev/libev_config.h",
      "TFYOCSSRKit/shadowsocks-libev/libev/*.{h,c}",
      "TFYOCSSRKit/shadowsocks-libev/libcork/**/*.{h,c}"  # 直接包含 libcork 文件
    ]
    
    ss.public_header_files = "TFYOCSSRKit/shadowsocks-libev/**/*.h"
    
    ss.preserve_paths = [
      'TFYOCSSRKit/shadowsocks-libev/libcork/include/**',
      'TFYOCSSRKit/shadowsocks-libev/libipset/include/**',
      'TFYOCSSRKit/shadowsocks-libev/libev_config.h',
      'TFYOCSSRKit/shadowsocks-libev/libev/**',
      'TFYOCSSRKit/shadowsocks-libev/src/include/**'
    ]
    
    ss.header_mappings_dir = "TFYOCSSRKit/shadowsocks-libev"
    
    ss.pod_target_xcconfig = {
      'HEADER_SEARCH_PATHS' => [
        '$(inherited)',
        '"$(PODS_TARGET_SRCROOT)/TFYOCSSRKit/shadowsocks-libev"',
        '"$(PODS_TARGET_SRCROOT)/TFYOCSSRKit/shadowsocks-libev/libcork/include"',
        '"$(PODS_TARGET_SRCROOT)/TFYOCSSRKit/shadowsocks-libev/libipset/include"',
        '"$(PODS_TARGET_SRCROOT)/TFYOCSSRKit/shadowsocks-libev/src/include"'
      ].join(' '),
      'GCC_PREPROCESSOR_DEFINITIONS' => 'HAVE_CONFIG_H=1'
    }
    
    ss.dependency 'TFYOCSSRKit/libopenssl'
    ss.dependency 'TFYOCSSRKit/libsodium'
    
    ss.compiler_flags = '-DHAVE_CONFIG_H'
  end
  
  spec.subspec 'Privoxy' do |ss|
    ss.source_files = "TFYOCSSRUilt/TFYOCSSRKit/Privoxy/**/*.{h,m,c}"
    ss.public_header_files = "TFYOCSSRUilt/TFYOCSSRKit/Privoxy/**/*.h"
  end
  
  spec.requires_arc = true

  # 添加依赖
  spec.frameworks = "Foundation", "UIKit"

  # 修改全局编译设置
  base_config = {
    'HEADER_SEARCH_PATHS' => [
      '$(inherited)',
      '"${PODS_ROOT}/Headers/Public"',
      '"${SRCROOT}/TFYOCSSRKit/shadowsocks-libev/libcork/include"',
      '"${SRCROOT}/TFYOCSSRKit/shadowsocks-libev/libipset/include"',
      '"${SRCROOT}/TFYOCSSRKit/libopenssl/include"',
      '"${SRCROOT}/TFYOCSSRKit/libsodium/include"',
      '"${SRCROOT}/TFYOCSSRKit"'
    ].join(' ')
  }
  
  spec.xcconfig = base_config

  # 全局编译标志
  spec.pod_target_xcconfig = { 
    'OTHER_CFLAGS' => '-DHAVE_CONFIG_H -DUSE_CRYPTO_OPENSSL -DLIB_ONLY -DUDPRELAY_LOCAL -DMODULE_LOCAL -DUSE_CRYPTO_MBEDTLS',
    'GCC_PREPROCESSOR_DEFINITIONS' => [
      'HAVE_CONFIG_H=1',
      'USE_CRYPTO_OPENSSL=1',
      'USE_CRYPTO_MBEDTLS=1',
      'LIB_ONLY=1',
      'UDPRELAY_LOCAL=1',
      'MODULE_LOCAL=1'
    ].join(' '),
    'CLANG_ENABLE_MODULES' => 'NO'
  }

end
