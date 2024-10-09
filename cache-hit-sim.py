import numpy as np
import matplotlib.pyplot as plt
import math
from matplotlib.animation import FuncAnimation
from matplotlib.colors import ListedColormap

class Cache:
    CACHE_EMPTY = 0
    IN_CACHE = 1
    CACHE_HIT = 2
    CACHE_MISS = 3
    MEM_LOAD = 4
    CACHE_V = (1 << 32)

    def __init__(self, memory, line_size, set_count, way_count):
        self.memory = np.zeros(memory, dtype=int)
        self.line_size = line_size
        self.set_count = set_count
        self.way_count = way_count
        self.cache_lines = np.zeros(set_count * way_count, dtype=int)  # V|Tag
        self.access_time = np.zeros(set_count * way_count, dtype=int)  # For LRU replacement policy
        self.hit_count = 0
        self.miss_count = 0
        self.time = 0  # Simulation time
    
    def cacul_set(self, address):
        return (address & (self.set_count * self.line_size - 1)) >> int(math.log(self.line_size, 2))
    
    def cacul_tag(self, address):
        return address >> int(math.log(self.set_count * self.line_size, 2))
    
    def cacul_addr(self, set, tag):
        return tag << int(math.log(self.set_count * self.line_size, 2)) | set << int(math.log(self.line_size, 2))

    def evict_line(self, set_index):
        oldest_time = self.time
        for i in range(0, self.way_count):
            if self.access_time[set_index * self.way_count + i] < oldest_time:
                oldest_time = self.access_time[set_index * self.way_count + i]
                oldest_index = i
        self.access_time[set_index * self.way_count + oldest_index] = 0;
        address = self.cacul_addr(set_index, self.cache_lines[set_index * self.way_count + oldest_index])
        self.cache_lines[set_index * self.way_count + oldest_index] = 0
        self.memory[address:address + self.line_size] = self.CACHE_EMPTY
        return oldest_index

    def flush_memory(self):
        self.memory.fill(self.CACHE_EMPTY)
        for i in range(self.set_count * self.way_count):
            if self.cache_lines[i] & self.CACHE_V == self.CACHE_V:
                address = self.cacul_addr(i // self.way_count, self.cache_lines[i] & ~self.CACHE_V)
                self.memory[address:address + self.line_size] = self.IN_CACHE

    def read(self, address, read_size):
        tag = self.cacul_tag(address)
        set = self.cacul_set(address)
        empty = -1
        
        self.flush_memory()
        self.time += 1

        for i in range(self.way_count):
            if (tag | self.CACHE_V) == self.cache_lines[set * self.way_count + i]:
                self.hit_count += 1
                self.memory[address:address + read_size] = self.CACHE_HIT
                self.access_time[set * self.way_count + i] = self.time
                print("Cache hit, tag: %x, set: %d, way: %d" % (tag, set, i))
                return
            else: 
                if self.cache_lines[set * self.way_count + i] & self.CACHE_V != self.CACHE_V:
                    empty = i

        if empty != -1:
            addr_aligned = address & ~(self.line_size - 1)
            self.cache_lines[set * self.way_count + empty] = self.CACHE_V | tag
            self.access_time[set * self.way_count + empty] = self.time
            self.memory[addr_aligned:addr_aligned + self.line_size] = self.MEM_LOAD
            self.memory[address:address + read_size] = self.CACHE_MISS
            print("Cache miss, tag: %x, set: %d, way: %d" % (tag, set, empty))
        else:
            evict_index = self.evict_line(set)
            self.cache_lines[set * self.way_count + evict_index] = self.CACHE_V | tag
            self.access_time[set * self.way_count + evict_index] = self.time
            addr_aligned = address & ~(self.line_size - 1) 
            self.memory[addr_aligned:addr_aligned + self.line_size] = self.MEM_LOAD
            self.memory[address:address + read_size] = self.CACHE_MISS
            print("Cache evict, tag: %x, set: %d, way: %d" % (tag, set, evict_index))
        
        self.miss_count += 1


    def get_memory_map(self):
        return self.memory

def visualize_cache(cache, matrix_size, element_size):
    cmap = ListedColormap(['white', 'lightgreen', 'green', 'red', 'pink'])

    fig, ax = plt.subplots()
    im = ax.imshow(cache.get_memory_map().reshape(matrix_size, matrix_size * element_size), cmap=cmap, vmin=0, vmax=4)
    
    def update(frame):
        if frame == 0:
            cache.flush_memory()
            im.set_data(cache.get_memory_map().reshape(matrix_size, matrix_size * element_size))
            return [im]
        i, j = divmod(frame - 1, matrix_size)
        address = ((j * matrix_size + i) * element_size)
        print("Access:x=%d, y=%d, addr=0x%x" % (i, j, address))
        cache.read(address, element_size)
        im.set_data(cache.get_memory_map().reshape(matrix_size, matrix_size * element_size))
        return [im]

    ani = FuncAnimation(fig, update, frames=range(0, matrix_size * matrix_size), interval=0, blit=True)
    plt.show()

def main(matrix_side, line_size, set_count, way_count, element_size=4):
    cache = Cache(memory=matrix_side * matrix_side * element_size, line_size=line_size, set_count=set_count, way_count=way_count)

    visualize_cache(cache, matrix_side, element_size)
    print("Hit count:", cache.hit_count)
    print("Miss count:", cache.miss_count)
    print("Hit rate:", cache.hit_count / (cache.hit_count + cache.miss_count))

if __name__ == "__main__":
    main(matrix_side=129, line_size=64, set_count=128, way_count=1, element_size=1)
