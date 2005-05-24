#!/usr/bin/env python 
'''
Copyright (C) 2005 Aaron Spike, aaron@ekips.org

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
'''
import inkex, cubicsuperpath, simplestyle, copy, math, bezmisc

def numsegs(csp):
	return sum([len(p)-1 for p in csp])
def interpcoord(v1,v2,p):
	return v1+((v2-v1)*p)
def interppoints(p1,p2,p):
	return [interpcoord(p1[0],p2[0],p),interpcoord(p1[1],p2[1],p)]
def pointdistance((x1,y1),(x2,y2)):
	return math.sqrt(((x2 - x1) ** 2) + ((y2 - y1) ** 2))
def bezlenapprx(sp1, sp2):
	return pointdistance(sp1[1], sp1[2]) + pointdistance(sp1[2], sp2[0]) + pointdistance(sp2[0], sp2[1])
def tpoint((x1,y1), (x2,y2), t = 0.5):
	return [x1+t*(x2-x1),y1+t*(y2-y1)]
def cspbezsplit(sp1, sp2, t = 0.5):
	m1=tpoint(sp1[1],sp1[2],t)
	m2=tpoint(sp1[2],sp2[0],t)
	m3=tpoint(sp2[0],sp2[1],t)
	m4=tpoint(m1,m2,t)
	m5=tpoint(m2,m3,t)
	m=tpoint(m4,m5,t)
	return [[sp1[0][:],sp1[1][:],m1], [m4,m,m5], [m3,sp2[1][:],sp2[2][:]]]
def cspbezsplitatlength(sp1, sp2, l = 0.5, tolerance = 0.001):
	bez = (sp1[1][:],sp1[2][:],sp2[0][:],sp2[1][:])
	t = bezmisc.beziertatlength(bez, l, tolerance)
	return cspbezsplit(sp1, sp2, t)
def cspseglength(sp1,sp2, tolerance = 0.001):
	bez = (sp1[1][:],sp1[2][:],sp2[0][:],sp2[1][:])
	return bezmisc.bezierlength(bez, tolerance)	
def csplength(csp):
	total = 0
	lengths = []
	for sp in csp:
		lengths.append([])
		for i in xrange(1,len(sp)):
			l = cspseglength(sp[i-1],sp[i])
			lengths[-1].append(l)
			total += l			
	return lengths, total

class Interp(inkex.Effect):
	def __init__(self):
		inkex.Effect.__init__(self)
		self.OptionParser.add_option("-e", "--exponent",
						action="store", type="float", 
						dest="exponent", default=0.0,
						help="values other than zero give non linear interpolation")
		self.OptionParser.add_option("-s", "--steps",
						action="store", type="int", 
						dest="steps", default=5,
						help="number of interpolation steps")
		self.OptionParser.add_option("-m", "--method",
						action="store", type="int", 
						dest="method", default=2,
						help="method of interpolation")
		self.OptionParser.add_option("-d", "--dup",
						action="store", type="inkbool", 
						dest="dup", default=True,
						help="duplicate endpaths")	
	def effect(self):
		exponent = self.options.exponent
		if exponent>= 0:
			exponent = 1.0 + exponent
		else:
			exponent = 1.0/(1.0 - exponent)
		steps = [1.0/(self.options.steps + 1.0)]
		for i in range(self.options.steps - 1):
			steps.append(steps[0] + steps[-1])
		steps = [step**exponent for step in steps]
			
		paths = {}
		for id in self.options.ids:
			node = self.selected[id]
			if node.tagName =='path':
				paths[id] = cubicsuperpath.parsePath(node.attributes.getNamedItem('d').value)
			else:
				self.options.ids.remove(id)

		for i in range(1,len(self.options.ids)):
			start = copy.deepcopy(paths[self.options.ids[i-1]])
			end = copy.deepcopy(paths[self.options.ids[i]])

			#which path has fewer segments?
			lengthdiff = numsegs(start) - numsegs(end)
			if lengthdiff > 0:
				start, end = end, start
				if len(steps) == 1:
					steps[0] = 1.0 - steps[0]

			if self.options.method == 2:
				#subdivide both paths into segments of relatively equal lengths
				slengths, stotal = csplength(start)
				elengths, etotal = csplength(end)
				lengths = {}
				t = 0
				for sp in slengths:
					for l in sp:
						t += l / stotal
						lengths.setdefault(t,0)
						lengths[t] += 1
				t = 0
				for sp in elengths:
					for l in sp:
						t += l / etotal
						lengths.setdefault(t,0)
						lengths[t] += -1
				sadd = [k for (k,v) in lengths.iteritems() if v < 0]
				sadd.sort()
				eadd = [k for (k,v) in lengths.iteritems() if v > 0]
				eadd.sort()

				t = 0
				s = [[]]
				for sp in slengths:
					if not start[0]:
						s.append(start.pop(0))
					s[-1].append(start[0].pop(0))
					for l in sp:
						pt = t
						t += l / stotal
						if sadd and t > sadd[0]:
							while sadd and sadd[0] < t:
								nt = (sadd[0] - pt) / (t - pt)
								bezes = cspbezsplitatlength(s[-1][-1][:],start[0][0][:], nt)
								s[-1][-1:] = bezes[:2]
								start[0][0] = bezes[2]
								pt = sadd.pop(0)
						s[-1].append(start[0].pop(0))
				t = 0
				e = [[]]
				for sp in elengths:
					if not end[0]:
						e.append(end.pop(0))
					e[-1].append(end[0].pop(0))
					for l in sp:
						pt = t
						t += l / etotal
						if eadd and t > eadd[0]:
							while eadd and eadd[0] < t:
								nt = (eadd[0] - pt) / (t - pt)
								bezes = cspbezsplitatlength(e[-1][-1][:],end[0][0][:], nt)
								e[-1][-1:] = bezes[:2]
								end[0][0] = bezes[2]
								pt = eadd.pop(0)
						e[-1].append(end[0].pop(0))
				start = s[:]
				end = e[:]
			else:
				#subdivide the shorter path
				for x in range(abs(lengthdiff)):
					maxlen = 0
					subpath = 0
					segment = 0
					for y in range(len(start)):
						for z in range(1,len(start[y])):
							leng = bezlenapprx(start[y][z-1],start[y][z])
							if leng > maxlen:
								maxlen = leng
								subpath = y
								segment = z
					sp1, sp2 = start[subpath][segment - 1:segment+1]
					start[subpath][segment - 1:segment+1] = cspbezsplit(sp1, sp2)
			
			#break paths so that corresponding subpaths have an equal number of segments
			s = [[]]
			e = [[]]
			while start and end:
				if start[0] and end[0]:
					s[-1].append(start[0].pop(0))
					e[-1].append(end[0].pop(0))
				elif end[0]:
					s.append(start.pop(0))
					e[-1].append(end[0][0])
					e.append([end[0].pop(0)])
				elif start[0]:
					e.append(end.pop(0))
					s[-1].append(start[0][0])
					s.append([start[0].pop(0)])
				else:
					s.append(start.pop(0))
					e.append(end.pop(0))
	
			if self.options.dup:
				steps = [0] + steps + [1]	
			#create an interpolated path for each interval
			group = self.document.createElement('svg:g')	
			self.document.documentElement.appendChild(group)
			for time in steps:
				interp = []
				#process subpaths
				for ssp,esp in zip(s,e):
					if not (ssp or esp):
						break
					interp.append([])
					#process superpoints
					for sp,ep in zip(ssp,esp):
						if not (sp or ep):
							break
						interp[-1].append([])
						#process points
						for p1,p2 in zip(sp,ep):
							if not (sp or ep):
								break
							interp[-1][-1].append(interppoints(p1,p2,time))

				#remove final subpath if empty.
				if not interp[-1]:
					del interp[-1]
				new = self.document.createElement('svg:path')
				style = {'stroke-linejoin': 'miter', 'stroke-width': '1.0px', 
					'stroke-opacity': '1.0', 'fill-opacity': '1.0', 
					'stroke': '#000000', 'stroke-linecap': 'butt', 
					'fill': 'none'}
				new.setAttribute('style', simplestyle.formatStyle(style))
				new.setAttribute('d', cubicsuperpath.formatPath(interp))
				group.appendChild(new)

e = Interp()
e.affect()
