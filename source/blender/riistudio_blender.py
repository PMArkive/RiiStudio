# src\boilerplate\bl_info.py

bl_info = {
	"name": "RiiStudio Blender Exporter",
	"author": "riidefi",
	"version": (1, 0),
	"blender": (2, 80, 0),
	"location": "File > Export",
	"description": "Export to BRRES/BMD files.",
	"warning": "Experimental Build",
	"wiki_url": "https://github.com/riidefi/RiiStudio",
	"tracker_url": "",
	"category": "Export"
}

# src\imports.py

import struct
import bpy, bmesh
import os, shutil
import mathutils
from bpy_extras.io_utils import axis_conversion
from bpy_extras.io_utils import ImportHelper, ExportHelper
from bpy.props import StringProperty, BoolProperty, EnumProperty, FloatProperty, IntProperty
from bpy.types import Operator
from collections import OrderedDict
from time import perf_counter
import subprocess
import mmap
import cProfile

BLENDER_30 = bpy.app.version[0] >= 3
BLENDER_28 = (bpy.app.version[0] == 2 and bpy.app.version[1] >= 80) \
	or BLENDER_30

# Adapted from
# https://blender.stackexchange.com/questions/7890/add-a-filter-for-the-extension-of-a-file-in-the-file-browser
class FilteredFiledialog(bpy.types.Operator, ImportHelper):
	bl_idname = "pathload.test"
	bl_label = 'Select .rspreset'
	filename_ext = '.rspreset'
	filter_glob = StringProperty(
		default="*.rspreset",
		options={'HIDDEN'},
		maxlen=255,  # Max internal buffer length, longer would be clamped.
	)
	if BLENDER_30: filter_glob : filter_glob

	def execute(self, context):
		setattr(self.string_prop_namespace, self.string_prop_name, bpy.path.relpath(self.filepath))
		return {'FINISHED'}

	def invoke(self, context, event):
		return super().invoke(context, event)

	@classmethod
	def add(cls, layout, string_prop_namespace, string_prop_name):
		cls.string_prop_namespace = string_prop_namespace
		cls.string_prop_name = string_prop_name
		col = layout.split(factor=.33)
		col.label(text=string_prop_namespace.bl_rna.properties[string_prop_name].name)
		row = col.row(align=True)
		if string_prop_namespace.bl_rna.properties[string_prop_name].subtype != 'NONE':
			row.label(text="ERROR: Change subtype of {} property to 'NONE'".format(string_prop_name), icon='ERROR')
		else:
			row.prop(string_prop_namespace, string_prop_name, icon_only=True)
			row.operator(cls.bl_idname, icon='FILE_TICK' if BLENDER_28 else 'FILESEL')

def get_user_prefs(context):
	return context.preferences if BLENDER_28 else context.user_preferences			

def get_rs_prefs(context):
	return get_user_prefs(context).addons[__name__].preferences

def invoke_converter(context, source, dest):
	bin_root = os.path.abspath(get_rs_prefs(context).riistudio_directory)
	tests_exe = os.path.join(bin_root, "tests.exe")
	
	subprocess.call([tests_exe, source, dest])

RHST_DATA_NULL   = 0

RHST_DATA_DICT   = 1
RHST_DATA_ARRAY  = 2
RHST_DATA_ARRAY_DYNAMIC = 3

RHST_DATA_END_DICT   = 4
RHST_DATA_END_ARRAY  = 5
RHST_DATA_END_ARRAY_DYNAMIC = 6

RHST_DATA_STRING = 7
RHST_DATA_S32	= 8
RHST_DATA_F32	= 9

DEBUG = False

class Timer:
	def __init__(self, name):
		self.name = name
		self.start = perf_counter()

	def dump(self):
		stop = perf_counter()
		delta = stop - self.start

		print("[%s] Elapsed %s seconds (%s ms)" % (self.name, delta, delta * 1000))

gOpenStreams = []

# RHST (Rii Hierarchical Scene Tree) is a high-throughput,
# multipurpose bitstream format I thought of the other day.
class RHSTWriter:
	class RHSTStream:
		def __init__(self, capacity, path):
			self.__pos = 0
			self.__max_size = 0
			with open(path, 'wb') as file:
				file.truncate(0)
			self.__file = open(path, "r+b")
			self.__buffer = mmap.mmap(self.__file.fileno(), capacity, access=mmap.ACCESS_WRITE)

			gOpenStreams.append(self)

		def write(self, data, size):
			self.__buffer[self.__pos : self.__pos + size] = data
			self.__pos += size
			
		#	def seek(self, position, whence):
		#		assert whence == 0
		#		self.__pos = position

		def tell(self):
			return self.__pos

		def close(self):
			self.__buffer.close()
			self.__file.close()

			gOpenStreams.remove(self)
			
	def __init__(self, path):
		self.__stream = self.RHSTStream(100 * 1000 * 1000, path)

		# Write header
		self.__write_bytes("RHST")
		self.__write_s32(1)

	def close(self):
		self.__write_s32(RHST_DATA_NULL)
		self.__stream.close()
		
	def __write_s32(self, val):
		self.__stream.write(struct.pack("<i", val), 4)

	def __write_f32(self, val):
		self.__stream.write(struct.pack("<f", val), 4)

	def __write_bytes(self, string):
		for val in string:
			self.__stream.write(struct.pack("<c", bytes(val, 'ascii')), 1)

	def __align(self, alignment):
		while self.__stream.tell() % alignment:
			self.__stream.write(bytes([0]), 1)

	def __write_inline_string(self, name):
		self.__write_s32(len(name))
		self.__write_bytes(name)
		self.__align(4)

	def write_s32(self, num):
		self.__stream.write(struct.pack("<ii", RHST_DATA_S32, num), 8)
		
	def write_f32(self, num):
		self.__stream.write(struct.pack("<if", RHST_DATA_F32, num), 8)

	def write_string(self, string):
		self.__stream.write(struct.pack("<ii", RHST_DATA_STRING, len(string)), 8)
		self.__write_bytes(string)
		self.__align(4)

	def begin_object(self, name, size):
		self.__stream.write(struct.pack("<iii", RHST_DATA_DICT, size, len(name)), 12)
		self.__write_bytes(name)
		self.__align(4)

		return True

	def end_object(self):
		self.__stream.write(struct.pack("<i", RHST_DATA_END_DICT), 4)

	def begin_array(self, size, type):
		self.__stream.write(struct.pack("<iii", RHST_DATA_ARRAY, size, type), 12)

		return True

	def end_array(self):
		self.__stream.write(struct.pack("<i", RHST_DATA_END_ARRAY), 4)
		
	def from_py(self, obj):
		x = type(obj)

		if x == float:
			self.write_f32(obj)
		elif x == int:
			self.write_s32(obj)
		elif x == str:
			self.write_string(obj)
		elif x == list or x == tuple:
			self.begin_array(len(obj), 0)
			for item in obj:
				self.from_py(item)
			self.end_array()
		elif x == dict or x == OrderedDict:
			#print(obj['name'])
			self.begin_object(obj['name'], len(obj))
			for k, v in obj.items():
				#assert isinstance(k, str)
				self.begin_object(k, 1)
				self.from_py(v)
				self.end_object()
			self.end_object()
		elif x == bool:
			self.write_s32(obj)
		else:
			print(type(obj))
			raise RuntimeError("Invalid type!")

# src\helpers\best_tex_format.py

def best_tex_format(tex):
	optimal_format = "?"
	if tex.brres_guided_color == 'color':
		if tex.brres_guided_color_transparency == 'opaque':
			if tex.brres_guided_optimize == 'quality':
				optimal_format = 'rgb565'
			else:
				optimal_format = 'cmpr'
		elif tex.brres_guided_color_transparency == 'outline':
			if tex.brres_guided_optimize == 'quality':
				optimal_format = 'rgb5a3'
			else:
				optimal_format = 'cmpr'
		else:
			if tex.brres_guided_optimize == 'quality':
				optimal_format = 'rgba8'
			else:
				optimal_format = 'rgb5a3'
	else:
		if tex.brres_guided_grayscale_alpha == 'use_alpha':
			if tex.brres_guided_optimize == 'quality':
				optimal_format = 'ia8'
			else:
				optimal_format = 'ia4'
		else:
			if tex.brres_guided_optimize == 'quality':
				optimal_format = 'i8'
			else:
				optimal_format = 'i4'
	return optimal_format

texture_format_items = (
	('i4', "Intensity 4-bits (I4)", "4 Bits/Texel - 16 Levels of Translucence - 8x8 Tiles"),
	('i8', "Intensity 8-bits (I8)", "8 Bits/Texel - 256 Levels of Translucence - 8x4 Tiles"),
	('ia4', "Intensity+Alpha 8-bits (IA4)", "8 Bits/Texel - 16 Levels of Translucence - 8x4 Tiles"),
	('ia8', "Intensity+Alpha 16-bits (IA8)", "16 Bits/Texel - 256 Levels of Translucence - 4x4 Tiles"),
	('rgb565', "RGB565", "16 Bits/Texel - No Transparency - 4x4 Tiles"),
	('rgb5a3', "RGB5A3", "16 Bits/Texel - 8 Levels of Translucence - 4x4 Tiles"),
	('rgba8', "RGBA8", "32 Bits/Texel - 256 Levels of Translucence - 4x4 Tiles"),
	('cmpr', "Compresed Texture (CMPR)", "4 Bits/Texel  - 0 Levels of Translucence - 8x8 Tiles")
)

def get_filename_without_extension(file_path):
	file_basename = os.path.basename(file_path)
	filename_without_extension = file_basename.split('.')[0]
	return filename_without_extension

# src\helpers\export_tex.py

def export_tex(texture, out_folder):
	tex_name = get_filename_without_extension(texture.image.name) if BLENDER_28 else texture.name
	print("ExportTex: %s" % tex_name)
	# Force PNG
	texture.image.file_format = 'PNG'
	# save image as PNNG
	texture_outpath = os.path.join(out_folder, tex_name) + ".png"
	tex0_outpath = os.path.join(out_folder, tex_name) + ".tex0"
	texture.image.save_render(texture_outpath)
	# determine format
	# print(best_tex_format(texture))
	tformat_string = (
		texture.brres_manual_format if texture.brres_mode == 'manual' else best_tex_format(
			texture)).upper()
	# determine mipmaps
	mm_string = ""
	if texture.brres_mipmap_mode == 'manual':
		mm_string = "--n-mm=%s" % texture.brres_mipmap_manual
	elif texture.brres_mipmap_mode == 'none':
		mm_string = "--n-mm=0"
	else:  # auto
		mm_string = "--mipmap-size=%s" % texture.brres_mipmap_minsize

# src\panels\BRRESTexturePanel.py

class BRRESTexturePanel(bpy.types.Panel):
	"""
	Set Wii Image Format for image encoding on JRES export
	"""
	bl_label = "RiiStudio Texture Options"
	bl_idname = "TEXTURE_PT_rstudio"
	bl_space_type = 'NODE_EDITOR' if BLENDER_28 else 'PROPERTIES'
	bl_region_type = 'UI' if BLENDER_28 else 'WINDOW'
	bl_category = "Item" if BLENDER_28 else ''
	bl_context = "node" if BLENDER_28 else 'texture'

	@classmethod
	def poll(cls, context):
		if BLENDER_28:
			return context.active_node and context.active_node.bl_idname == 'ShaderNodeTexImage'
		
		return context.texture and context.texture.type == 'IMAGE' and context.texture.image

	def draw(self, context):
		tex = context.active_node if BLENDER_28 else context.texture
		layout = self.layout
		c_box = layout.box()
		c_box.label(text="Caching", icon='FILE_IMAGE')
		c_box.row().prop(tex, "jres_is_cached")
		mm_box = layout.box()
		mm_box.label(text="Mipmaps", icon='RENDERLAYERS')
		col = mm_box.column()
		col.row().prop(tex, 'brres_mipmap_mode', expand=True)
		if tex.brres_mipmap_mode == 'manual':
			col.prop(tex, 'brres_mipmap_manual')
		elif tex.brres_mipmap_mode == 'auto':
			col.prop(tex, 'brres_mipmap_minsize')
		else:
			col.label(text="No mipmapping will be performed")
		tf_box = layout.box()
		tf_box.label(text="Wii Texture Format", icon='TEXTURE_DATA')
		row = tf_box.row()
		row.prop(tex, "brres_mode", expand=True)
		if tex.brres_mode == 'guided':
			box = tf_box.box()
			col = box.column()
			col.prop(tex, "brres_guided_optimize", expand=False)
			row = box.row()
			row.prop(tex, "brres_guided_color", expand=True)
			# col = box.column()
			row = box.row()
			optimal_format = "?"
			if tex.brres_guided_color == 'color':
				row.prop(tex, "brres_guided_color_transparency", expand=True)
				row = box.row()
				if tex.brres_guided_color_transparency == 'opaque':
					if tex.brres_guided_optimize == 'quality':
						optimal_format = 'rgb565'
					else:
						optimal_format = 'cmpr'
				elif tex.brres_guided_color_transparency == 'outline':
					if tex.brres_guided_optimize == 'quality':
						optimal_format = 'rgb5a3'
					else:
						optimal_format = 'cmpr'
				else:
					if tex.brres_guided_optimize == 'quality':
						optimal_format = 'rgba8'
					else:
						optimal_format = 'rgb5a3'
			else:
				row.prop(tex, "brres_guided_grayscale_alpha", expand=True)
				if tex.brres_guided_grayscale_alpha == 'use_alpha':
					if tex.brres_guided_optimize == 'quality':
						optimal_format = 'ia8'
					else:
						optimal_format = 'ia4'
				else:
					if tex.brres_guided_optimize == 'quality':
						optimal_format = 'i8'
					else:
						optimal_format = 'i4'
			# tex.guided_determined_best = optimal_format
			box2 = box.box()
			optimal_format_display = "?"
			optimal_format_display2 = "?"
			for item in texture_format_items:
				if item[0] == optimal_format:
					optimal_format_display = item[1]
					optimal_format_display2 = item[2]
			box2.row().label(text='Optimal Format: %s' % optimal_format_display)
			box2.row().label(text='(%s)' % optimal_format_display2)
		else:
			box = layout.box()
			col = box.column()
			col.label(text="Texture format")
			col.prop(tex, "brres_manual_format", expand=True)


# src\panels\JRESMaterialPanel.py

class JRESMaterialPanel(bpy.types.Panel):
	"""
	Set material options for JRES encoding
	"""
	bl_label = "RiiStudio Material Options"
	bl_idname = "MATERIAL_PT_rstudio"
	bl_space_type = 'PROPERTIES'
	bl_region_type = 'WINDOW'
	bl_context = "material"

	@classmethod
	def poll(cls, context):
		return context.material

	def draw(self, context):
		layout = self.layout
		mat = context.material

		# Culling
		box = layout.box()
		box.label(text="Culling", icon='MOD_WIREFRAME')
		row = box.row(align=True)
		row.prop(mat, "jres_display_front", toggle=True)
		row.prop(mat, "jres_display_back", toggle=True)

		# PE
		box = layout.box()
		box.label(text="Pixel Engine", icon='IMAGE_ALPHA')
		row = box.row(align=True)
		row.prop(mat, "jres_pe_mode", expand=True)

		# Lighting
		box = layout.box()
		box.label(text="Lighting", icon='OUTLINER_OB_LIGHT' if BLENDER_28 else 'LAMP_SUN')  # Might want to change icon here
		box.row().prop(mat, "jres_lightset_index")

		# Fog
		box = layout.box()
		box.label(text="Fog", icon='RESTRICT_RENDER_ON')
		box.row().prop(mat, "jres_fog_index")

		# UV Wrapping
		box = layout.box()
		box.label(text="UV Wrapping Mode", icon='GROUP_UVS')
		row = box.row(align=True)
		row.prop(mat, "jres_wrap_u")
		row.prop(mat, "jres_wrap_v")

		# Material Preset
		box = layout.box()
		box.label(text="Material Presets (Experimental)", icon='ERROR')
		box.row().label(text="Create .rspreset files in RiiStudio by right clicking on a material > Create Preset.")
		box.row().label(text="These files will contain animations, too.")
		# box.row().prop(mat, 'preset_path_mdl0mat_or_rspreset')
		FilteredFiledialog.add(box.row(), mat, 'preset_path_mdl0mat_or_rspreset')


# src\panels\JRESScenePanel.py

class JRESScenePanel(bpy.types.Panel):
	"""
	Currently for texture caching
	"""
	bl_label = "RiiStudio Scene Options"
	bl_idname = "SCENE_PT_rstudio"
	bl_space_type = 'PROPERTIES'
	bl_region_type = 'WINDOW'
	bl_context = "scene"

	@classmethod
	def poll(cls, context):
		return context.scene

	def draw(self, context):
		layout = self.layout
		scn = context.scene

		# Caching
		box = layout.box()
		box.label(text="Caching", icon='FILE_IMAGE')
		row = box.row(align=True)
		row.prop(scn, "jres_cache_dir")


# src\exporters\jres\export_jres.py
def vec2(x):
	return (x.x, x.y)
def vec3(x):
	return (x.x, x.y, x.z)
def vec4(x):
	return (x.x, x.y, x.z, x.w)

def all_objects():
	if BLENDER_28:
		for Collection in bpy.data.collections:
			for Object in Collection.objects:
				prio = 0
				flags = list(Collection.name.split(':'))
				if len(flags) > 1:
					prio = int(flags[1])
				print(prio)
				yield Object, prio
	else:
		for Object in bpy.data.objects:
			# Only objects in the leftmost layers are exported
			lxe = any(Object.layers[0:5] + Object.layers[10:15])
			if not lxe:
				print("Object %s excluded as not in left layers" % Object.name)
				continue
			prio = 0
			yield Object, prio

def all_meshes():
	for obj, prio in all_objects():
		if obj.type == 'MESH':
			yield obj, prio

def get_texture(mat):
	if BLENDER_28:
		for n in mat.node_tree.nodes:
			if n.bl_idname == "ShaderNodeTexImage":
				return n

		raise RuntimeError("Cannot find active texture for material %s" % mat.name)
	
	return mat.active_texture

def all_tex_uses():
	for Object, prio in all_meshes():
		for slot in Object.material_slots:
			mat = slot.material
			if mat is None:
				print("Object %s does not have a material, skipping" % Object.name)
				continue

			tex = get_texture(mat)
			if not tex:
				continue
			
			yield tex

def all_textures():
	return set(all_tex_uses())

def export_textures(textures_path):
	if not os.path.exists(textures_path):
		os.makedirs(textures_path)

	for tex in all_textures():
		export_tex(tex, textures_path)

def build_rs_mat(mat, texture_name):
	return {
		'name': mat.name,
		# Texture element soon to be replaced with texmap array
		'texture': texture_name,
		"wrap_u": mat.jres_wrap_u,
		"wrap_v": mat.jres_wrap_v,
		# Culling / Display Surfaces
		'display_front': mat.jres_display_front,
		'display_back': mat.jres_display_back,
		'pe': mat.jres_pe_mode,
		'lightset': mat.jres_lightset_index,
		'fog': mat.jres_fog_index,
		# For compatibility, this field is not changed in RHST
		# It can specify mdl0mat OR rspreset
		'preset_path_mdl0mat': bpy.path.abspath(mat.preset_path_mdl0mat_or_rspreset),
	}

def mesh_from_object(Object):
	if BLENDER_28:
		depsgraph = bpy.context.evaluated_depsgraph_get()
		object_eval = Object.evaluated_get(depsgraph)
		return bpy.data.meshes.new_from_object(object_eval)

	return Object.to_mesh(context.scene, True, 'PREVIEW', calc_tessface=False, calc_undeformed=True)

def export_mesh(
	Object,
	magnification,
	split_mesh_by_material,
	add_dummy_colors,
	context,
	model,
	prio
):
	triangulated = None
	try:
		triangulated = mesh_from_object(Object)
	except:
		print("Failed to triangulate object %s!" % Object.name)
		return
	# Triangulate:
	bm = bmesh.new()
	bm.from_mesh(triangulated)
	bmesh.ops.triangulate(bm, faces=bm.faces)
	bm.to_mesh(triangulated)
	bm.free()

	axis = axis_conversion(to_forward='-Z', to_up='Y',).to_4x4()
	global_matrix = (mathutils.Matrix.Scale(magnification, 4) @ axis) if BLENDER_28 else (mathutils.Matrix.Scale(magnification, 4) * axis)

	triangulated.transform(global_matrix @ Object.matrix_world if BLENDER_28 else global_matrix * Object.matrix_world)
	triangulated.flip_normals()
	'''
	triangulated.transform(mathutils.Matrix.Scale(magnification, 4))
	quat = Object.matrix_world.to_quaternion()
	quat.rotate(mathutils.Quaternion((1, 0, 0), math.radians(270)).to_matrix())
	triangulated.transform(quat.to_matrix().to_4x4())
	'''
	has_vcolors = len(triangulated.vertex_colors)

	ColorInputs = [-1, -1]
	for x in range(len(triangulated.vertex_colors[:2])):
		ColorInputs[x] = 0
	
	if add_dummy_colors and ColorInputs[0] == -1:
		ColorInputs[0] = 0

	UVInputs = [-1, -1, -1, -1, -1, -1, -1, -1]
	for x in range(len(triangulated.uv_layers[:8])):
		UVInputs[x] = 0

	for mat_index, mat in enumerate(triangulated.materials):
		if mat is None:
			print("ERR: Object %s has materials with unassigned materials?" % Object.name)
			continue
		# for tri in triangulated.polygons:
		# if tri.material_index == mat_index:
		# Draw Calls format: [material_index, polygon_index, priority]

		# TODO: manually assign priority in object attribs
		texture_name = 'default_material'
		if BLENDER_28:
			if get_texture(mat):
				texture_name = get_filename_without_extension(get_texture(mat).image.name)
		else:
			if mat and mat.active_texture:
				texture_name = mat.active_texture.name

		vcd_set = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
		polygon_object = OrderedDict({
			"name": "%s___%s" % (Object.name, texture_name),
			"primitive_type": "triangle_fan",
			"current_matrix": 0,
			"facepoint_format": vcd_set})
		polygon_object["matrix_primitives"] = []

		vcd_set[9] = vcd_set[10] = 1
		vcd_set[11:13] = [int(i > -1) for i in ColorInputs] if has_vcolors else [int(add_dummy_colors), 0]
		vcd_set[13:21] = [int(i > -1) for i in UVInputs]

		# we'll worry about this when we have to, 1 primitive array should be fine for now.
		facepoints = [] # [ [ V, N, C0, C1, U0, U1, U2, U3, U4, U5, U6, U7 ], ... ]
		num_verts = len(triangulated.polygons) * 3
		for idx, tri in zip(range(0, num_verts, 3), triangulated.polygons):
			#print(idx)
			#print(tri)
			if tri.material_index != mat_index and split_mesh_by_material:
				# print("Skipped because tri mat: %u, target: %u" % (tri.material_index, mat_index))
				continue
			for global_index, fpVerticeIndex in enumerate(tri.vertices, idx):
				#print(global_index, fpVerticeIndex)
				blender_vertex = triangulated.vertices[fpVerticeIndex]
				gvertex = [vec3(blender_vertex.co), vec3(blender_vertex.normal)]
				if has_vcolors:
					for layer in triangulated.vertex_colors[:2]:
						# TODO: Is this less if smaller? Add data not index
						clr = layer.data[global_index].color
						gvertex += [tuple(attr for attr in clr)]
				elif add_dummy_colors:
					gvertex += [[1.0, 1.0, 1.0, 1.0]]
				for layer in triangulated.uv_layers[:8]:
					raw_uv = vec2(layer.data[global_index].uv)
					gvertex += [(raw_uv[0], 1 - raw_uv[1])]
				facepoints.append(gvertex)		

		if not len(facepoints):
			print("No vertices: skipping")
			continue

		if not mat:
			print("No material: skipping")
			continue

		if not get_texture(mat):
			print("No texture: skipping")
			continue

		polygon_object["matrix_primitives"].append({
			"name": "N/A",
			"matrix": [-1, -1, -1, -1, -1, -1, -1, -1, -1, -1],
			"primitives": [{
				"name": "N/A",
				"primitive_type": "triangles",
				"facepoints": facepoints
			}]
		})
		mesh_id = model.add_mesh(polygon_object)

		material_object = build_rs_mat(mat, texture_name)
		mat_id = model.add_material(material_object)
		
		model.append_drawcall(mat_id, mesh_id, prio=prio)

		# All mesh data will already be exported if not being split. This stops duplicate data
		if not split_mesh_by_material:
			break

class SRT:
	def __init__(self, s=(1.0, 1.0, 1.0), r=(0.0, 0.0, 0.0), t=(0.0, 0.0, 0.0)):
		self.s = s
		self.r = r
		self.t = t


class Quantization:
	def __init__(self, pos="float", nrm="float", uv="float", clr="auto"):
		self.pos = pos
		self.nrm = nrm
		self.uv  = uv
		self.clr = clr


class ConverterFlags:
	def __init__(self, split_mesh_by_material=True, mesh_conversion_mode='PREVIEW',
		add_dummy_colors = True, ignore_cache = False, write_metadata = False):
		
		self.split_mesh_by_material = split_mesh_by_material
		self.mesh_conversion_mode = mesh_conversion_mode
		self.add_dummy_colors = add_dummy_colors
		self.ignore_cache = ignore_cache
		self.write_metadata = False

class RHSTExportParams:
	def __init__(self, dest_path, quantization=Quantization(), root_transform = SRT(),
				magnification=1000, flags = ConverterFlags()):
		self.dest_path = dest_path
		self.quantization = quantization
		self.root_transform = root_transform
		self.magnification = magnification
		self.flags = flags


def export_jres(context, params : RHSTExportParams):
	rhst = RHSTWriter(params.dest_path)

	rhst.begin_object("root", 2)

	rhst.from_py({'name': "head", 'generator': "RiiStudio Blender", 'type': "JMDL", 'version': "Beta 1"})

	current_data = {
		"materials": [],
		"polygons": [],
		"weights": [  # matrix array
			[  # weight array
				[0, 100]  # The weight
			]
		],
		"bones": [{
			"name": "riistudio_blender%s" % bpy.app.version[1],
			"parent": -1,
			"child": -1,
			"scale": params.root_transform.s,
			"rotate": params.root_transform.r,
			"translate": params.root_transform.t,
			"min": [0, 0, 0],
			"max": [0, 0, 0],
			"billboard": "none",
			"draws": []
		}]
	}
	class Model:
		def __init__(self, current_data):
			self.object_i = 0
			self.current_data = current_data
			self.material_remap = {}

		def alloc_mesh_id(self):
			cur_id = self.object_i
			self.object_i += 1
			return cur_id

		def append_drawcall(self, mat, poly, prio):
			self.current_data["bones"][0]["draws"].append([mat, poly, prio])

		def add_mesh(self, poly):
			self.current_data["polygons"].append(poly)
			return self.alloc_mesh_id()

		def add_material(self, mat):
			materials = self.current_data["materials"]
			tex_name = mat["texture"]

			if tex_name in self.material_remap:
				return self.material_remap[tex_name]
			
			new_mi = len(materials)

			self.current_data["materials"].append(mat)
			self.material_remap[tex_name] = len(materials) - 1
			return new_mi

	model = Model(current_data)

	for Object, prio in all_meshes():
		export_mesh(
			Object,
			params.magnification,
			params.flags.split_mesh_by_material,
			params.flags.add_dummy_colors,
			context,
			model,
			prio
		)

	current_data['name'] = 'body'

	start = perf_counter()

	rhst.from_py(current_data)
	#cProfile.runctx("rhst.from_py(current_data)", globals(), locals())
	
	rhst.end_object() # "root"
	rhst.close()

	end = perf_counter()
	delta = end - start
	print("Serialize took %u sec, %u msec" % (delta, delta * 1000))

class RHST_RNA:
	quantize_types = (
		("float", "float", "Higher precision"),
		("fixed", "fixed", "Lower precision")  # ,
		# ("auto", "auto", "Allow converter to choose quantization")
	)
	position_quantize = EnumProperty(
		name="Position",
		default="float",
		items=quantize_types
	)
	if BLENDER_30: position_quantize : position_quantize

	normal_quantize = EnumProperty(
		name="Normal",
		default="float",
		items=(
			("float", "float", "Highest precision"),
			("fixed14", "fixed14", "Fixed-14 precision"),
			("fixed6", "fixed6", "Lowest precision")
		)
	)
	if BLENDER_30: normal_quantize : normal_quantize

	uv_quantize = EnumProperty(
		name="UV",
		default="float",
		items=quantize_types
	)
	if BLENDER_30: uv_quantize : uv_quantize

	color_quantize = EnumProperty(
		name="Color",
		default='rgb8',
		items=(
			('rgba8', "rgba8", "8-bit RGBA channel (256 levels)"),
			('rgba6', "rgba6", "6-bit RGBA channel (64 levels)"),
			('rgba4', "rgba4", "4-bit RGBA channel (16 levels)"),
			('rgb8', "rgb8", "8-bit RGB channel (256 levels)"),
			('rgb565', "rgb565", "5-bit RB channels (32 levels), and 6-bit G channel (64 levels)")
		)
	)
	if BLENDER_30: color_quantize : color_quantize

	root_transform_scale_x = FloatProperty(name="X", default=1)
	root_transform_scale_y = FloatProperty(name="Y", default=1)
	root_transform_scale_z = FloatProperty(name="Z", default=1)
	root_transform_rotate_x = FloatProperty(name="X", default=0)
	root_transform_rotate_y = FloatProperty(name="Y", default=0)
	root_transform_rotate_z = FloatProperty(name="Z", default=0)
	root_transform_translate_x = FloatProperty(name="X", default=0)
	root_transform_translate_y = FloatProperty(name="Y", default=0)
	root_transform_translate_z = FloatProperty(name="Z", default=0)
	if BLENDER_30:
		root_transform_scale_x : root_transform_scale_x
		root_transform_scale_y : root_transform_scale_y
		root_transform_scale_z : root_transform_scale_z
		root_transform_rotate_x : root_transform_rotate_x
		root_transform_rotate_y : root_transform_rotate_y
		root_transform_rotate_z : root_transform_rotate_z
		root_transform_translate_x : root_transform_translate_x
		root_transform_translate_y : root_transform_translate_y
		root_transform_translate_z : root_transform_translate_z

	magnification = FloatProperty(
		name="Magnification",
		default=1000
	)
	if BLENDER_30: magnification : magnification

	split_mesh_by_material = BoolProperty(name="Split Mesh by Material", default=True)
	if BLENDER_30: split_mesh_by_material : split_mesh_by_material
	
	mesh_conversion_mode = EnumProperty(
		name="Mesh Conversion Mode",
		default='PREVIEW',
		items=(
			('PREVIEW', "Preview", "Preview settings"),
			('RENDER', "Render", "Render settings"),
		)
	)
	if BLENDER_30: mesh_conversion_mode : mesh_conversion_mode

	add_dummy_colors = BoolProperty(
		name="Add Dummy Vertex Colors",
		description="Allows for polygons without assigned vertex colors to use the same materials as polygons with assigned vertex colors",
		default=True
	)
	if BLENDER_30: add_dummy_colors : add_dummy_colors

	ignore_cache = BoolProperty(
		name="Ignore Cache",
		default=False,
		description="Ignore the cache and rebuild every texture"
	)
	if BLENDER_30: ignore_cache : ignore_cache

	keep_build_artifacts = BoolProperty(
		name="Keep Build Artifacts",
		default=False,
		description="Don't delete .rhst and .png files"
	)
	if BLENDER_30: keep_build_artifacts : keep_build_artifacts

	def get_root_transform(self):
		root_scale	 = [self.root_transform_scale_x,	 self.root_transform_scale_y,	 self.root_transform_scale_z]
		root_rotate	= [self.root_transform_rotate_x,	self.root_transform_rotate_y,	self.root_transform_rotate_z]
		root_translate = [self.root_transform_translate_x, self.root_transform_translate_y, self.root_transform_translate_z]

		return SRT(root_scale, root_rotate, root_translate)

	def get_quantization(self):
		return Quantization(
			pos = self.position_quantize,
			nrm = self.normal_quantize,
			uv  = self.uv_quantize,
			clr = self.color_quantize
		)

	def get_converter_flags(self):
		return ConverterFlags(
			self.split_mesh_by_material,
			self.mesh_conversion_mode,
			self.add_dummy_colors,
			self.ignore_cache
		)

	def get_rhst_path(self):
		return os.path.join(os.path.split(self.filepath)[0], "course.rhst")

	def get_textures_path(self):
		return os.path.join(os.path.split(self.filepath)[0], "textures")

	def get_dest_path(self):
		return self.filepath

	def get_export_params(self):
		tmp_path = self.get_rhst_path()

		root_transform = self.get_root_transform()
		quantization = self.get_quantization()
		converter_flags = self.get_converter_flags()

		return RHSTExportParams(tmp_path,
			quantization   = quantization,
			root_transform = root_transform,
			magnification  = self.magnification,
			flags		  = converter_flags
		)

	def draw_rhst_options(self, context):
		layout = self.layout
		# Mesh
		box = layout.box()
		box.label(text="PMesh", icon='MESH_DATA')
		box.prop(self, "magnification", icon='VIEWZOOM' if BLENDER_28 else 'MAN_SCALE')
		box.prop(self, "split_mesh_by_material")
		box.prop(self, "mesh_conversion_mode")
		box.prop(self, 'add_dummy_colors')
		box.prop(self, 'ignore_cache')
		box.prop(self, 'keep_build_artifacts')

		# Quantization
		box = layout.box()
		box.label(text="Quantization", icon='LINENUMBERS_ON')
		col = box.column()
		col.prop(self, "position_quantize")
		col.prop(self, "normal_quantize")
		col.prop(self, "uv_quantize")
		col.prop(self, "color_quantize")
		
		# Root Transform
		box = layout.box()
		box.label(text="Root Transform", icon='FULLSCREEN_ENTER' if BLENDER_28 else 'MANIPUL')
		row = box.row(align=True)
		row.label(text="Scale")
		row.prop(self, "root_transform_scale_x")
		row.prop(self, "root_transform_scale_y")
		row.prop(self, "root_transform_scale_z")
		row = box.row(align=True)
		row.label(text="Rotate")
		row.prop(self, "root_transform_rotate_x")
		row.prop(self, "root_transform_rotate_y")
		row.prop(self, "root_transform_rotate_z")
		row = box.row(align=True)
		row.label(text="Translate")
		row.prop(self, "root_transform_translate_x")
		row.prop(self, "root_transform_translate_y")
		row.prop(self, "root_transform_translate_z")

	def export_rhst(self, context, dump_pngs=True):
		try:
			timer = Timer("RHST Generation")

			export_jres(
				context,
				self.get_export_params()
			)
			timer.dump()

			if dump_pngs:
				# Dump .PNG images
				timer = Timer("PNG Dumping")
				export_textures(self.get_textures_path())
				timer.dump()
		finally:
			for stream in gOpenStreams:
				stream.close()

	def cleanup_rhst(self):
		os.remove(self.get_rhst_path())
		# shutil.rmtree(self.get_textures_path())

	def cleanup_if_enabled(self):
		if not self.keep_build_artifacts:
			self.cleanup_rhst()

# src\exporters\brres\ExportBRRESCap.py

class ExportBRRES(Operator, ExportHelper, RHST_RNA):
	"""Export file as BRRES"""
	bl_idname = "rstudio.export_brres"
	bl_label = "Blender BRRES Exporter"
	bl_options = {'PRESET'}
	filename_ext = ".brres"

	filter_glob = StringProperty(
		default="*.brres",
		options={'HIDDEN'},
		maxlen=255,  # Max internal buffer length, longer would be clamped.
	)
	if BLENDER_30: filter_glob : filter_glob

	def draw(self, context):
		box = self.layout.box()
		box.label(text="BRRES", icon='FILE_TICK' if BLENDER_28 else 'FILESEL')
		
		self.draw_rhst_options(context)

	def export(self, context, format):
		try:
			self.export_rhst(context, dump_pngs=True)
			
			timer = Timer("BRRES Conversion")
			invoke_converter(context, source=self.get_rhst_path(), dest=self.get_dest_path())
			timer.dump()
		finally:
			self.cleanup_if_enabled()
		
	def execute(self, context):
		timer = Timer("BRRES Export")
		
		self.export(context, 'BRRES')

		timer.dump()
				
		return {'FINISHED'}

class ExportBMD(Operator, ExportHelper, RHST_RNA):
	"""Export file as BMD"""
	bl_idname = "rstudio.export_bmd"
	bl_label = "Blender BMD Exporter"
	bl_options = {'PRESET'}
	filename_ext = ".bmd"

	filter_glob = StringProperty(
		default="*.bmd",
		options={'HIDDEN'},
		maxlen=255,  # Max internal buffer length, longer would be clamped.
	)
	if BLENDER_30: filter_glob : filter_glob

	def draw(self, context):
		box = self.layout.box()
		box.label(text="BMD", icon='FILE_TICK' if BLENDER_28 else 'FILESEL')
		
		self.draw_rhst_options(context)

	def export(self, context, format):
		try:
			self.export_rhst(context, dump_pngs=True)
			
			timer = Timer("BMD Conversion")
			invoke_converter(context, source=self.get_rhst_path(), dest=self.get_dest_path())
			timer.dump()
		finally:
			self.cleanup_if_enabled()
	
	def execute(self, context):
		timer = Timer("BMD Export")
		self.export(context, 'BMD')
		timer.dump()
				
		return {'FINISHED'}

# Only needed if you want to add into a dynamic menu
def brres_menu_func_export(self, context):
	self.layout.operator(ExportBRRES.bl_idname, text="BRRES (RiiStudio)")
def bmd_menu_func_export(self, context):
	self.layout.operator(ExportBMD.bl_idname, text="BMD (RiiStudio)")


# src\preferences.py

def make_rs_path_absolute():
	prefs = get_rs_prefs(bpy.context)

	if prefs.riistudio_directory.startswith('//'):
		prefs.riistudio_directory = os.path.abspath(bpy.path.abspath(prefs.riistudio_directory))

class RiidefiStudioPreferenceProperty(bpy.types.AddonPreferences):
	bl_idname = __name__

	riistudio_directory = bpy.props.StringProperty(
		name="RiiStudio Directory",
		description="Folder of RiiStudio.exe",
		subtype='DIR_PATH',
		update = lambda s,c: make_rs_path_absolute(),
		default=""
	)
	if BLENDER_30: riistudio_directory : riistudio_directory

	def draw(self, context):
		layout = self.layout
		box = layout.box()
		box.label(text="RiiStudio Folder", icon='FILE_IMAGE')
		box.row().prop(self, "riistudio_directory")
		if not BLENDER_28:
			layout.label(text="Don't forget to save user preferences!")



class OBJECT_OT_addon_prefs_example(bpy.types.Operator):
	"""Display example preferences"""
	bl_idname = "object.rstudio_prefs_operator"
	bl_label = "Addon Preferences Example"
	bl_options = {'REGISTER', 'UNDO'}

	def execute(self, context):
		user_preferences = context.user_preferences
		addon_prefs = user_preferences.addons[__name__].preferences

		info = ("riistudio_directory: %s" % addon_prefs.riistudio_directory)

		self.report({'INFO'}, info)
		print(info)

		return {'FINISHED'}

# src\base.py

classes = (
	FilteredFiledialog,
	ExportBRRES,
	ExportBMD,

	BRRESTexturePanel,
	JRESMaterialPanel,
	# JRESScenePanel,

	RiidefiStudioPreferenceProperty,
	OBJECT_OT_addon_prefs_example
)

UV_WRAP_MODES = (
	('repeat', "Repeat", "Repeated texture; requires texture be ^2"),
	('mirror', "Mirror", "Mirrored-Repeated texture; requires texture be ^2"),
	('clamp',  "Clamp",  "Clamped texture; does not require texture be ^2")
)


def register_tex():
	tex_type = bpy.types.Node if BLENDER_28 else bpy.types.Texture

	tex_type.brres_mode = EnumProperty(
		default='guided',
		items=(
			('guided', 'Guided', 'Guided Texture setting'),
			('manual', 'Manual', 'Manually specify format')
		)
	)
	tex_type.brres_guided_optimize = EnumProperty(
		name="Optimize for",
		items=(
			('quality', 'Quality', 'Optimize for quality'), ('filesize', 'Filesize', 'Optimize for lowest filesize')),
		default='filesize'
	)
	tex_type.brres_guided_color = EnumProperty(
		name="Color Type",
		items=(
			('color', 'Color', 'Color Image'),
			('grayscale', 'Grayscale', 'grayscale (No Color) Image')
		),
		default='color'
	)
	tex_type.brres_guided_color_transparency = EnumProperty(
		name="Transparency Type",
		default='opaque',
		items=(
			('opaque', "Opaque", "Opaque (No Transparency) Image"),
			('outline', "Outline", "Outline (Binary Transparency) Image"),
			('translucent', "Translucent", "Translucent (Full Transparent) Image")
		)
	)
	tex_type.brres_guided_grayscale_alpha = EnumProperty(
		name="Uses Alpha",
		default='use_alpha',
		items=(
			('use_alpha', 'Uses transparency', 'The image uses transparency'),
			('no_alpha', 'Does\'t use transparency', 'The image does not use transparency')
		)
	)
	tex_type.brres_manual_format = EnumProperty(
		items=texture_format_items
	)
	tex_type.brres_mipmap_mode = EnumProperty(
		items=(
			('auto', "Auto", "Allow the conversion tool to determine best mipmapping level (currently wimgt)"),
			('manual', "Manual", "Specify the number of mipmaps"),
			('none', "None", "Do not perform mipmapping (the same as manual > 0)")
		),
		default='auto',
		name="Mode"
	)
	tex_type.brres_mipmap_manual = IntProperty(
		name="#Mipmap",
		default=0
	)
	tex_type.brres_mipmap_minsize = IntProperty(
		name="Minimum Mipmap Size",
		default=32
	)

def register_mat():
	# Display Surfaces
	bpy.types.Material.jres_display_front = BoolProperty(
		name="Display Front",
		default=True
	)
	bpy.types.Material.jres_display_back = BoolProperty(
		name="Display Back",
		default=False
	)
	# PE and Blend Modes
	bpy.types.Material.jres_pe_mode = EnumProperty(
		name="PE Mode",
		items=(
			('opaque', "Opaque", "No alpha"),
			('outline', "Outline", "Binary alpha. A texel is either opaque or fully transparent"),
			('translucent', "Translucent", "Expresses a full range of alpha")
		),
		default='opaque'
	)
	# Lighting
	bpy.types.Material.jres_lightset_index = IntProperty(
		name="Lightset Index",
		default=-1
	)
	# Fog
	bpy.types.Material.jres_fog_index = IntProperty(
		name="Fog Index",
		default=0
	)

	# UV Wrapping
	bpy.types.Material.jres_wrap_u = EnumProperty(
		name="U",
		items=UV_WRAP_MODES,
		default='repeat'
	)
	bpy.types.Material.jres_wrap_v = EnumProperty(
		name="V",
		items=UV_WRAP_MODES,
		default='repeat'
	)

	# Presets
	#
	# This field can specify:
	# 1) a .mdl0mat preset (path of a FOLDER with .mdl0mat/.mdl0shade/.tex0*/.srt0*
	# 2) or a .rsmat preset (path of a FILE with all included)
	#
	# For now it only selects .rspreset files, but should be trivial to support .mdl0mat too
	# -> `subtype='DIR_PATH'` will instruct blender to select a folder. 
	#
	# Perhaps ideally we'd configure a presets folder and have a drop-down.
	# 
	bpy.types.Material.preset_path_mdl0mat_or_rspreset = StringProperty(
		name="Preset Path",
		subtype='NONE', # Custom FilteredFiledialog
	)

def register():
	MT_file_export = bpy.types.TOPBAR_MT_file_export if BLENDER_28 else bpy.types.INFO_MT_file_export
	MT_file_export.append(brres_menu_func_export)
	MT_file_export.append(bmd_menu_func_export)
	
	register_tex()
	register_mat()

	# Texture Cache
	tex_type = bpy.types.Node if BLENDER_28 else bpy.types.Texture
	tex_type.jres_is_cached = BoolProperty(
		name="Is cached? Uncheck when changes are made",
		default=False
	)
	#	# Scene Cache
	#	bpy.types.Scene.jres_cache_dir = StringProperty(
	#		name="Cache Directory Subname",
	#		subtype='DIR_PATH'
	#	)

	for c in classes:
		bpy.utils.register_class(c)


def unregister():
	for c in classes:
		bpy.utils.unregister_class(c)
	MT_file_export = bpy.types.TOPBAR_MT_file_export if BLENDER_28 else bpy.types.INFO_MT_file_export
	MT_file_export.remove(brres_menu_func_export)
	MT_file_export.remove(bmd_menu_func_export)


def main():
	register()
	# test call
	bpy.ops.rstudio.export_brres('INVOKE_DEFAULT')

if __name__ == "__main__":
	main()
