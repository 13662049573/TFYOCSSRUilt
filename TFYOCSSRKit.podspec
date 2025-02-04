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
  

  spec.source_files  = "TFYOCSSRUilt/TFYOCSSRKit/**/*.{h,m}"
 
  spec.requires_arc = true

  # 添加依赖
  spec.frameworks = "Foundation", "UIKit"

end
