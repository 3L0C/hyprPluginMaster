all:
	$(CXX) -DWLR_USE_UNSTABLE -shared -fPIC --no-gnu-unique main.cpp PluginMasterLayout.cpp -o masterLayoutPlugin.so -g `pkg-config --cflags pixman-1 libdrm hyprland` -std=c++2b
clean:
	rm ./masterLayoutPlugin.so
