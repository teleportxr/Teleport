import sys
import glob
import os
import subprocess
import struct
def BinToCpp(input_filename,output_filename):
	varName=str.replace(output_filename,'.cpp','')
	with open(output_filename,'wb') as result_file:
		result_file.write(b'\tstatic const unsigned int %sSPIRV[] = {\n\t\t'% varName.encode('utf-8') )
		f=open(input_filename, 'rb')
		line_counter=int(8);
		while 1:
			byte_4=f.read(4)
			if not byte_4:
				break
			integer = struct.unpack('I', byte_4)
			result_file.write(b'0x%08X,' % integer)
			line_counter-=1
			if line_counter==0:
				result_file.write(b'\n\t\t')
				line_counter=8
		result_file.write(b'};')


def run(args):
	wd=os.getcwd()
	print(wd)
	
	try:
		process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		out = process.stdout.readlines()
		for ln in out:
			print(ln)
		process.poll()
	except:
		pass

def Process(glsl):
	print(glsl)
	spirv_file=str.replace(str.replace(glsl,'.frag','.spv'),'/','\\');
	spirv_file=str.replace(str.replace(spirv_file,'.vert','.spv'),'/','\\');
	args=[os.environ['VULKAN_SDK']+'/Bin/glslangValidator.exe','-V','-entry-point main','-o '+spirv_file,glsl]
	print(' '.join(args))
	run(' '.join(args))
	print(spirv_file)
	cpp_file=str.replace(spirv_file,'.spv','.cpp')
	BinToCpp(spirv_file,cpp_file)

#glslangValidator -V -S vert -Os --entry-point VS_DepthWrite --source-entrypoint main -g -Od -o "DepthWrite_vv.spirv" "S_DepthWrite_vv.glsl"
#debug_VS_DepthWrite_vv.glsl(0): info: Temporary shader source file.
#debug_VS_DepthWrite_vv.glsl
for glsl in glob.glob("*.frag"):
	Process(glsl)
for glsl in glob.glob("*.vert"):
	Process(glsl)
