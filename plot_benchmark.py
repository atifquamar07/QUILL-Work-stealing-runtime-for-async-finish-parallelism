import matplotlib.pyplot as plt

x = [1, 2, 3, 4]
linear_speed = 15.305248
y = [linear_speed/15.305248, linear_speed/13.446473, linear_speed/13.473794, linear_speed/13.394865]

plt.figure()

plt.plot(x, y, 'bo')
plt.xlabel('QUILL_WORKERS')
plt.ylabel('TIME')

plt.plot([0, max(x)], [0, max(x)], 'k--')

plt.show()
