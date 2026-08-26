# Spaitotemporal Composability 项目 Makefile
# 编译整个Spaitotemporal库

# 编译器设置
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -fPIC
INCLUDES = -I.
LIBNAME = libspatiotemporal
VERSION = 1.0

# 源文件
SRC = spatiotemporal_types.c spatiotemporal_core.c spatiotemporal_analysis.c spatiotemporal_visualization.c spatiotemporal_api.c
OBJ = $(SRC:.c=.o)
HEADERS = spatiotemporal_types.h spatiotemporal_core.h spatiotemporal_analysis.h spatiotemporal_visualization.h spatiotemporal.h

# 目标文件
STATIC_LIB = $(LIBNAME).a
DYNAMIC_LIB = $(LIBNAME).so.$(VERSION)
SHARED_LIB = $(LIBNAME).so

# 默认目标
all: $(STATIC_LIB) $(DYNAMIC_LIB)

# 静态库
$(STATIC_LIB): $(OBJ)
	ar rcs $@ $^

# 动态库
$(DYNAMIC_LIB): $(OBJ)
	$(CC) -shared -Wl,-soname,$(LIBNAME).so.1 -o $@ $^

# 共享库符号链接
$(SHARED_LIB): $(DYNAMIC_LIB)
	ln -sf $(LIBNAME).so.$(VERSION) $(LIBNAME).so

# 目标文件
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 示例程序
examples: all
	$(CC) $(CFLAGS) -L. -I. examples/basic_usage.c -o basic_usage -lspatiotemporal
	$(CC) $(CFLAGS) -L. -I. examples/visualization_demo.c -o visualization_demo -lspatiotemporal
	$(CC) $(CFLAGS) -L. -I. examples/analysis_demo.c -o analysis_demo -lspatiotemporal

# 测试
test: all
	$(CC) $(CFLAGS) -L. -I. tests/test_core.c -o test_core -lspatiotemporal
	$(CC) $(CFLAGS) -L. -I. tests/test_analysis.c -o test_analysis -lspatiotemporal
	./test_core
	./test_analysis

# 安装
install: $(STATIC_LIB) $(DYNAMIC_LIB)
	install -d $(DESTDIR)/usr/local/lib
	install -d $(DESTDIR)/usr/local/include/spatiotemporal
	install -m 644 $(STATIC_LIB) $(DESTDIR)/usr/local/lib/
	install -m 644 $(DYNAMIC_LIB) $(DESTDIR)/usr/local/lib/
	install -m 644 $(HEADERS) $(DESTDIR)/usr/local/include/spatiotemporal/
	ln -sf $(LIBNAME).so.$(VERSION) $(DESTDIR)/usr/local/lib/$(LIBNAME).so

# 清理
clean:
	rm -f $(OBJ) $(STATIC_LIB) $(DYNAMIC_LIB) $(SHARED_LIB)
	rm -f basic_usage visualization_demo analysis_demo
	rm -f test_core test_analysis
	rm -f *.bmp *.png *.jpg *.json

# 依赖项
-include $(OBJ:.o=.d)

# 生成依赖项
%.d: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -MM -MT $(@:.d=.o) $< > $@

.PHONY: all clean install test examples