import matplotlib.pyplot as plt

x = [1, 2, 3, 4]
linear_speed = 16.355108
y = [linear_speed/16.355108, linear_speed/7.300798, linear_speed/5.754703, linear_speed/4.617367]

plt.figure()

plt.plot(x, y, 'bo')
plt.xlabel('QUILL_WORKERS')
plt.ylabel('TIME')

plt.plot([0, max(x)], [0, max(x)], 'k--')

plt.show()
