import jimara_file_tools, sys, os

required_argc = 3
instructions = (
	"Usage: python jimara_generate_lit_shaders.py merged_lights_src output_dir model_src_dir shader_src_dir <model_exts> <shader_exts> <out_ext>\n" +
	"    merged_lights_src - Lighting models require definitions for all light types, their attachments and Jimara_GetLightSamples function\n" +
	"                        generated by jimara_merge_light_shaders.py even if the model is unlit. Here we require the path to the generated file;\n" +
	"    output_dir        - Output file directory;\n"
	"    model_src_dir     - Directory to search lighting model source files in or a single lighting model file path;\n" +
	"    shader_src_dir    - Directory to search lit shader source files in to merge with the lighting models, or a single shader path;\n" +
	"                        Defaults to model_src_dir if it is a directory and thirds argument is not specified;\n"
	"    model_exts        - Comma-separated list of extensions to search for lighting models with.\n" + 
	"                        Ignored if model_src_dir is a single file; defaults to \"jlm\"<stands for \"Jimara Lighting Model\">;\n" +
	"    shader_exts       - Comma-separated list of extensions to search for shaders with.\n" +
	"                        Ignored if shader_src_dir is a single file; defaults to \"jls\"<stands for \"Jimara Lit Shader\">;\n" + 
	"    out_ext           - Extension to be used by the output file; defaults to \"glsl\".")


class job_arguments:
	def __init__(self, merged_lights_src, output_dir, model_src_dir, shader_src_dir, model_exts, frag_exts, out_ext):
		self.merged_lights_src = merged_lights_src
		self.output_dir = output_dir
		self.model_src_dir = model_src_dir
		self.shader_src_dir = shader_src_dir
		self.model_exts = model_exts
		self.frag_exts = frag_exts
		self.out_ext = out_ext

	def incomplete(self):
		return (
			(self.merged_lights_src is None) or 
			(self.output_dir is None) or 
			(self.model_src_dir is None) or 
			(self.shader_src_dir is None) or 
			(self.model_exts is None) or 
			(self.frag_exts is None) or
			(self.out_ext is None))

	def __str__(self):
		return (
			"jimara_generate_lit_shaders - JOB ARGUMENTS:\n" +
			"    merged_lights_src - " + repr(self.merged_lights_src) + "\n" +
			"    output_dir        - " + repr(self.output_dir) + "\n" +
			"    model_src_dir     - " + repr(self.model_src_dir) + "\n" +
			"    shader_src_dir    - " + repr(self.shader_src_dir) + "\n" +
			"    model_exts        - " + str(self.model_exts) + "\n" +
			"    frag_exts         - " + str(self.frag_exts) + "\n" +
			"    out_ext           - " + str(self.out_ext) + "\n")

def make_job_arguments(args = sys.argv[1:]):
	model_src_dir = None if (len(args) <= 2) else args[2]
	return job_arguments(
		None if (len(args) <= 0) else args[0],
		None if (len(args) <= 1) else args[1],
		model_src_dir,
		((model_src_dir if ((model_src_dir is not None) and os.path.isdir(model_src_dir)) else None) if (len(args) <= 3) else args[3]),
		("jlm" if (len(args) <= 4) else args[4]).split(","),
		("jls" if (len(args) <= 5) else args[5]).split(","),
		"glsl" if (len(args) <= 6) else args[6])


class task_description:
	def __init__(self, model, shader, model_dir, shader_dir, out_dir, out_ext = "frag"):
		self.model = model
		self.shader = shader
		self.output = os.path.join(
			os.path.join(out_dir, jimara_file_tools.strip_file_extension(os.path.relpath(model, model_dir))), 
			jimara_file_tools.strip_file_extension(os.path.relpath(shader, shader_dir)) + "." + out_ext)

	def __str__(self):
		return "<model: " + repr(self.model) + "; shader: " + repr(self.shader) + "; output: " + repr(self.output) + ">"


class job_description:
	def __init__(self, job_args):
		def find_files(directory, extensions):
			if os.path.isdir(directory):
				return directory, jimara_file_tools.find_by_extension(directory, extensions)
			elif os.path.isfile(directory):
				return "", [directory]
			else:
				return "", []
		self.light_src = job_args.merged_lights_src
		model_dir, self.model_src = find_files(job_args.model_src_dir, job_args.model_exts)
		frag_dir, self.frag_src = find_files(job_args.shader_src_dir, job_args.frag_exts)
		self.tasks = []
		for model in self.model_src:
			for shader in self.frag_src:
				self.tasks.append(task_description(model, shader, model_dir, frag_dir, job_args.output_dir, job_args.out_ext))
	
	def __str__(self):
		text = (
			"JOB_DESCRIPTION:\n" +
			"    light_src - " + self.light_src + "\n" + 
			"    tasks     - {\n")
		for i, task in enumerate(self.tasks):
			text += "         " + str(i) + ". " + str(task) + "\n"
		text += "}\n"
		return text
		
def execute_job(desc):
	file_cache = {}
	def read_file(path):
		if path in file_cache:
			return file_cache[path]
		try:
			with open(path, "r") as light:
				text = light.read()
		except:
			print("Error: Could not open file: " + path)
			return None
		file_cache[path] = text
		return text

	light_src = read_file(desc.light_src)
	if light_src is None:
		return

	def build_shader(model_src, shader_src):
		return (
			"/**\n" + 
			"################################################################################\n" +
			"######################### LIGHT TYPES AND DEFINITIONS: #########################\n" + 
			"*/\n" +
			light_src + "\n\n\n\n\n" +
			"/**\n" + 
			"################################################################################\n" +
			"################################ SHADER SOURCE: ################################\n" + 
			"*/\n" +
			"#define MATERIAL_BINDING_SET_ID (LIGHT_BINDING_SET_ID + 1)\n" +
			shader_src + "\n\n\n\n\n" +
			"/**\n" + 
			"################################################################################\n" +
			"############################ LIGHTING MODEL SOURCE: ############################\n" + 
			"*/\n" +
			"#define MODEL_BINDING_SET_ID LIGHT_BINDING_SET_ID\n"
			"#define MODEL_BINDING_START_ID LIGHT_BINDING_END_ID\n" +
			model_src + "\n")

	for task in desc.tasks:
		model_src = read_file(task.model)
		shader_src = read_file(task.shader)
		if (model_src is None) or (shader_src is None):
			print("Error: Could not read source files specified in task description: \n" + str(task))
			continue
		folder = os.path.dirname(task.output)
		if not os.path.isdir(folder):
			try:
				os.makedirs(folder)
			except:
				print("Error: Could not create directory " + repr(folder))
				continue
		with open(task.output, "w") as output:
			output.write(build_shader(model_src, shader_src))

if __name__ == "__main__":
	if len(sys.argv) <= required_argc:
		print(instructions)
		exit()

	args = make_job_arguments()

	if args.incomplete():
		print(instructions)
		exit()

	print(args)

	job = job_description(args)
	print(job)

	execute_job(job)
