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

    def __init__(self, memory, line_size, set_count, way_count):
        self.memory = np.zeros(memory, dtype=int)
        self.line_size = line_size
        self.set_count = set_count
        self.way_count = way_count
        self.cache_lines = np.zeros(set_count * way_count, dtype=int)  # 模拟缓存行
        self.access_time = np.zeros(set_count * way_count, dtype=int)  # 记录每个cache line的最近访问时间，用于LRU
        self.hit_count = 0
        self.miss_count = 0
        self.time = 0  # 模拟访问的时间，用于LRU替换策略
    
    def cacul_set(self, address):
        return (address & (self.set_count * self.line_size - 1)) >> int(math.log(self.line_size, 2))
    
    def cacul_tag(self, address):
        return address >> int(math.log(self.set_count * self.line_size, 2))
    
    def cacul_addr(self, set, tag):
        return tag << int(math.log(self.set_count * self.line_size, 2)) | set << int(math.log(self.line_size, 2))

    def evict_line(self, set_index):
        # 驱逐策略：LRU (最近最少使用)
        oldest_time = -1
        for i in range(self.way_count):
            if self.access_time[set_index * self.way_count + i] > oldest_time :
                oldest_time = self.access_time[set_index * self.way_count + i]
                oldest_index = i
        self.access_time[set_index * self.way_count + oldest_index] = 0;
        address = self.cacul_addr(set_index, self.cache_lines[set_index * self.way_count + oldest_index])
        self.cache_lines[set_index * self.way_count + oldest_index] = 0
        self.memory[address:address + self.line_size] = self.CACHE_EMPTY  # 显示白色
        return oldest_index

    def read(self, address, read_size):
        tag = self.cacul_tag(address)
        set = self.cacul_set(address)
        empty = -1
        
        print("Read: tag=%x, set=%d" % (tag, set))

        # 先查找缓存中是否有命中
        for i in range(self.way_count):
            if tag == self.cache_lines[set * self.way_count + i]:
                self.hit_count += 1
                self.memory[address:address + read_size] = self.CACHE_HIT  # 命中，显示深绿色
                self.access_time[set * self.way_count + i] = self.time  # 记录访问时间
                self.time += 1  # 记录时间
                print("Cache hit")
                return
            else: 
                if self.cache_lines[set * self.way_count + i] == 0:
                    empty = i  # 缓存中空闲的位置

        # 缓存中没有命中，需要加载缓存行
        if empty != -1:
            addr_aligned = address & ~(self.line_size - 1)  # 按行对齐
            self.cache_lines[set * self.way_count + empty] = tag
            self.memory[addr_aligned:addr_aligned + self.line_size] = self.MEM_LOAD
            self.memory[address:address + read_size] = self.CACHE_MISS
        else:
            # 缓存满了，需要驱逐缓存行
            evict_index = self.evict_line(set)
            self.cache_lines[set * self.way_count + evict_index] = tag
            addr_aligned = address & ~(self.line_size - 1)  # 按行对齐
            self.memory[addr_aligned:addr_aligned + self.line_size] = self.MEM_LOAD
            self.memory[address:address + read_size] = self.CACHE_MISS
            self.time += 1  # 记录时间
            self.miss_count += 1


    def get_memory_map(self):
        return self.memory

def visualize_cache(cache, matrix_size, element_size):
    cmap = ListedColormap(['white', 'lightgreen', 'green', 'red', 'pink'])

    fig, ax = plt.subplots()
    im = ax.imshow(cache.get_memory_map().reshape(matrix_size, matrix_size * element_size), cmap=cmap, vmin=0, vmax=4)
    
    def update(frame):
        i, j = divmod(frame * element_size, matrix_size)
        address = (j * matrix_size + i * element_size)  # 按列访问
        print("Access:x=%d, y=%d, addr=0x%x" % (i, j, address))
        cache.read(address, element_size)  # 每次读取一个元素及其后面的cache line
        im.set_data(cache.get_memory_map().reshape(matrix_size, matrix_size * element_size))
        return [im]

    ani = FuncAnimation(fig, update, frames=range(0, matrix_size * matrix_size // element_size), interval=100, blit=True)
    plt.show()

def main(matrix_side, line_size, set_count, way_count, element_size=4):
    # Cache data size is the matrix size squared
    cache = Cache(memory=matrix_side * matrix_side * element_size, line_size=line_size, set_count=set_count, way_count=way_count)

    # Start the visualization
    visualize_cache(cache, matrix_side, element_size)
    print("Hit count:", cache.hit_count)
    print("Miss count:", cache.miss_count)
    print("Hit rate:", cache.hit_count / (cache.hit_count + cache.miss_count))

if __name__ == "__main__":
    main(matrix_side=64, line_size=64, set_count=4, way_count=1, element_size=4)

