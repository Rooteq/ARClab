function [ qchd, qchd_d1, qchd_d2 ] = effectorTrajectoryGenerator3D( t, parameters)
%effectorTrajectoryGenerator
%generates prime function, first and second derivative for
%end effector trajectory

    % w = 0.015 * 2 * pi;
    % w = 0.030 * 2 * pi;
    % t_half = 50;
    t_total = 100;
    w = 2*pi / (t_total/2);
    k = 0.25;
    dx = 1.5;
    dy = 0.1;
    dz = 0.0;

    dx_2 = dx ;
    dy_2 = dy + 2*k;
    dz_2 = dz;

    switch_circle_time = t_total / 2;

    if t < switch_circle_time
        qchd = [-k*sin(w*t)+dx; k*cos(w*t)+dy; -0.1*k*sin(w*t)+dz];
        qchd_d1 = [-k*w*cos(w*t); -k*w*sin(w*t); -0.1 * k * w * cos(w*t)];
        qchd_d2 = [k*w^2*sin(w*t); -k*w^2*cos(w*t); 0.1 * k * w * sin(w*t)];
    else
        qchd = [k*sin(w*t)+dx_2; -k*cos(w*t)+dy_2; 0.1*k*sin(w*t)+dz_2];
        qchd_d1 = [k*w*cos(w*t); k*w*sin(w*t); 0.1 * k * w * cos(w*t)];
        qchd_d2 = [-k*w^2*sin(w*t); k*w^2*cos(w*t); -0.1 * k * w * sin(w*t)];
    end
    
    
end

