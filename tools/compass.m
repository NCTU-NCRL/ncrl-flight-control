%compass least square calibration
%input file name: compass.csv
%csv format for this script: mx,my,mz\n

%read sampling data from csv
csv = csvread("compass.csv");
mx = csv(:, 1);
my = csv(:, 2);
mz = csv(:, 3);

[r, c] = size(mx);
N = r; %N samplings
div_N = 1 / N;

%averaging sampling datas for calculating least square fitting ellipsoid
x_bar = div_N * sum(mx(:));
y_bar = div_N * sum(my(:));
z_bar = div_N * sum(mz(:));
xy_bar = div_N * sum(mx(:) .* my(:));
xz_bar = div_N * sum(mx(:) .* mz(:));
yz_bar = div_N * sum(my(:) .* mz(:));
xx_bar = div_N * sum(mx(:) .* mx(:));
yy_bar = div_N * sum(my(:) .* my(:));
zz_bar = div_N * sum(mz(:) .* mz(:));
xxy_bar = div_N * sum(mx(:) .* mx(:) .* my(:));
xxz_bar = div_N * sum(mx(:) .* mx(:) .* mz(:));
xyy_bar = div_N * sum(mx(:) .* my(:) .* my(:));
yyz_bar = div_N * sum(my(:) .* my(:) .* mz(:));
xzz_bar = div_N * sum(mx(:) .* mz(:) .* mz(:));
yzz_bar = div_N * sum(my(:) .* mz(:) .* mz(:));
xxx_bar = div_N * sum(mx(:) .* mx(:) .* mx(:));
yyy_bar = div_N * sum(my(:) .* my(:) .* my(:));
zzz_bar = div_N * sum(mz(:) .* mz(:) .* mz(:));
yyyy_bar = div_N * sum(my(:) .* my(:) .* my(:) .* my(:));
zzzz_bar = div_N * sum(mz(:) .* mz(:) .* mz(:) .* mz(:));
xxyy_bar = div_N * sum(mx(:) .* mx(:) .* my(:) .* my(:));
xxzz_bar = div_N * sum(mx(:) .* mx(:) .* mz(:) .* mz(:));
yyzz_bar = div_N * sum(my(:) .* my(:) .* mz(:) .* mz(:));

%solve the least square linear equation
_A = [yyyy_bar, yyzz_bar, xyy_bar, yyy_bar, yyz_bar, yy_bar;
      yyzz_bar, zzzz_bar, xzz_bar, yzz_bar, zzz_bar, zz_bar;
      xyy_bar,  xzz_bar,  xx_bar,  xy_bar,  xz_bar,  x_bar;
      yyy_bar,  yzz_bar,  xy_bar,  yy_bar,  yz_bar,  y_bar;
      yyz_bar,  zzz_bar,  xz_bar,  yz_bar,  zz_bar,  z_bar;
      yy_bar,   zz_bar,   x_bar,   y_bar,   z_bar,   1];

_b = [-xxyy_bar;
      -xxzz_bar;
      -xxx_bar;
      -xxy_bar;
      -xxz_bar;
      -xx_bar];

x = inv(_A) * _b;

%parameters of ellipsoid in general form
a = x(1)
b = x(2)
c = x(3)
d = x(4)
e = x(5)
f = x(6)

disp('-------------')

%parameters of ellipsoid in standard form
x0 = -0.5 * c
y0 = -d / (2.0 * a)
z0 = -e / (2.0 * b)
A = sqrt(x0*x0 + a*y0*y0 + b*z0*z0 - f)
B = A / sqrt(a)
C = A / sqrt(b)

%apply calibration
mx_calib = (mx(:) - x0) / A;
my_calib = (my(:) - y0) / B;
mz_calib = (mz(:) - z0) / C;

%%%%%%%%%%%%
% plotting %
%%%%%%%%%%%%

%raw data
figure(1);
scatter3(mx,my,mz)
%
xlim([-100, 100])
ylim([-100, 100])
zlim([-100, 100])
xlabel('mx (uT)')
ylabel('my (uT)')
zlabel('mz (uT)')
daspect([1 1 1]) %set aspect ratio to 1:1:1
grid on

%calibrated data
figure(2);
scatter3(mx_calib,my_calib,mz_calib)
%
xlim([-1, 1])
ylim([-1, 1])
zlim([-1, 1])
xlabel('mx')
ylabel('my')
zlabel('mz')
daspect([1 1 1]) %set aspect ratio to 1:1:1
grid on

disp('press any key to leave')
pause
