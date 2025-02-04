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
    ss.source_files = "TFYOCSSRUilt/TFYOCSSRKit/Antinat/**/*.{h,m,c}"
    ss.public_header_files = "TFYOCSSRUilt/TFYOCSSRKit/Antinat/**/*.h"
    ss.vendored_libraries = "TFYOCSSRUilt/TFYOCSSRKit/Antinat/expat-lib/lib/libexpat.a"
  end
  
  spec.subspec 'libmaxminddb' do |ss|
    ss.source_files = "TFYOCSSRUilt/TFYOCSSRKit/libmaxminddb/**/*.{h,m,c}"
    ss.public_header_files = "TFYOCSSRUilt/TFYOCSSRKit/libmaxminddb/**/*.h"
  end
  
  spec.subspec 'libopenssl' do |ss|
    ss.source_files = "TFYOCSSRUilt/TFYOCSSRKit/libopenssl/**/*.{h,m,c}"
    ss.public_header_files = "TFYOCSSRUilt/TFYOCSSRKit/libopenssl/**/*.h"
    ss.vendored_libraries = [
      "TFYOCSSRUilt/TFYOCSSRKit/libopenssl/lib/libcrypto.a",
      "TFYOCSSRUilt/TFYOCSSRKit/libopenssl/lib/libssl.a"
    ]
  end
  
  spec.subspec 'libsodium' do |ss|
    ss.source_files = "TFYOCSSRUilt/TFYOCSSRKit/libsodium/**/*.{h,m,c}"
    ss.public_header_files = "TFYOCSSRUilt/TFYOCSSRKit/libsodium/**/*.h"
    ss.vendored_libraries = "TFYOCSSRUilt/TFYOCSSRKit/libsodium/lib/libsodium_ios.a"
  end
  
  spec.subspec 'shadowsocks-libev' do |ss|
    ss.source_files = "TFYOCSSRUilt/TFYOCSSRKit/shadowsocks-libev/**/*.{h,m,c}"
    ss.public_header_files = "TFYOCSSRUilt/TFYOCSSRKit/shadowsocks-libev/**/*.h"
    ss.vendored_libraries = [
      "TFYOCSSRUilt/TFYOCSSRKit/shadowsocks-libev/libmbedtls/lib/libmbedcrypto.a",
      "TFYOCSSRUilt/TFYOCSSRKit/shadowsocks-libev/libmbedtls/lib/libmbedtls.a",
      "TFYOCSSRUilt/TFYOCSSRKit/shadowsocks-libev/libmbedtls/lib/libmbedx509.a"
    ]
  end
  
  spec.subspec 'Privoxy' do |ss|
    ss.source_files = "TFYOCSSRUilt/TFYOCSSRKit/Privoxy/**/*.{h,m,c}"
    ss.public_header_files = "TFYOCSSRUilt/TFYOCSSRKit/Privoxy/**/*.h"
  end
  
  spec.requires_arc = true

  # 添加依赖
  spec.frameworks = "Foundation", "UIKit"

  # 添加编译设置
  spec.xcconfig = {
    'HEADER_SEARCH_PATHS' => '$(inherited) "${PODS_ROOT}/Headers/Public"',
    'CLANG_ALLOW_NON_MODULAR_INCLUDES_IN_FRAMEWORK_MODULES' => 'YES',
    'LIBRARY_SEARCH_PATHS' => '$(inherited) "${PODS_ROOT}/**"'
  }

  spec.pod_target_xcconfig = { 
    'OTHER_CFLAGS' => '-DHAVE_CONFIG_H -DUSE_CRYPTO_OPENSSL -DLIB_ONLY -DUDPRELAY_LOCAL -DMODULE_LOCAL'
  }

end
