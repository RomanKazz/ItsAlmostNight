import Foundation
import CoreGraphics
import ImageIO
let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: CommandLine.arguments[1]) as CFURL,nil)!
let atlas = CGImageSourceCreateImageAtIndex(source,0,nil)!
let names = ["forest","desert","snow","corrupted","mountain","base","boss","boulders"]
let cellW = atlas.width/4, cellH = atlas.height/2
for (i,name) in names.enumerated() {
 let cell = atlas.cropping(to:CGRect(x:i%4*cellW,y:i/4*cellH,width:cellW,height:cellH))!
 let probe = CGContext(data:nil,width:cellW,height:cellH,bitsPerComponent:8,bytesPerRow:cellW*4,space:CGColorSpaceCreateDeviceRGB(),bitmapInfo:CGImageAlphaInfo.premultipliedLast.rawValue)!
 probe.draw(cell,in:CGRect(x:0,y:0,width:cellW,height:cellH))
 let bytes = probe.data!.assumingMemoryBound(to:UInt8.self)
 var minX=cellW,minY=cellH,maxX=0,maxY=0
 for y in 0..<cellH { for x in 0..<cellW { if bytes[(y*cellW+x)*4+3] > 10 {minX=min(minX,x);minY=min(minY,y);maxX=max(maxX,x);maxY=max(maxY,y)} } }
 // Tight alpha bounds plus two source pixels protect antialiased outlines.
 let crop = cell.cropping(to:CGRect(x:max(0,minX-2),y:max(0,minY-2),width:min(cellW-minX+2,maxX-minX+5),height:min(cellH-minY+2,maxY-minY+5)))!
 let ctx=CGContext(data:nil,width:256,height:256,bitsPerComponent:8,bytesPerRow:1024,space:CGColorSpaceCreateDeviceRGB(),bitmapInfo:CGImageAlphaInfo.premultipliedLast.rawValue)!
 ctx.interpolationQuality = .high
 let scale = 224.0/Double(max(crop.width,crop.height)), w=Double(crop.width)*scale,h=Double(crop.height)*scale
 ctx.draw(crop,in:CGRect(x:(256-w)/2,y:(256-h)/2,width:w,height:h))
 let dst=CGImageDestinationCreateWithURL(URL(fileURLWithPath:"assets/ui/map/\(name).png") as CFURL,"public.png" as CFString,1,nil)!
 CGImageDestinationAddImage(dst,ctx.makeImage()!,nil);CGImageDestinationFinalize(dst)
 print(name,crop.width,crop.height)
}
