clear
set(0,'defaultAxesFontSize',20)
set(0,'defaulttextInterpreter','latex');
set(groot, 'defaultAxesTickLabelInterpreter','latex');
set(groot, 'defaultLegendInterpreter','latex');
set(groot, 'defaultLineMarkerSize', 7);
set(groot, 'defaultLineLineWidth', 1);

%% load stuff
comp = [];
for i = [3,7,8]
    load(sprintf('accel_tests/spinup_%d_comp', i))
    obj.setcur = data(1,:);
    obj.t = data(2,:);
    obj.pos = data(3,:);
    obj.cur = data(4,:);
    obj.linestyle = 'b-';
    comp = [comp, obj];
end
nocomp = [];
for i = [4,5,6]
    load(sprintf('accel_tests/spinup_%d_nocomp', i))
    obj.setcur = data(1,:);
    obj.t = data(2,:);
    obj.pos = data(3,:);
    obj.cur = data(4,:);
    obj.linestyle = 'r-';
    nocomp = [nocomp, obj];
end

%% extract accelerations
for i = 1:length(comp)
    comp(i).setcurs = [];
    comp(i).as = [];
    for setcuri = 1:length(comp(i).setcur)
        P = polyfit(comp(i).t{setcuri}, comp(i).pos{setcuri} / 8192, 2);
        comp(i).a(setcuri) = 2*P(1);
        comp(i).setcurs = [comp(i).setcurs, comp(i).setcur{setcuri}*-(-1).^setcuri];
        comp(i).as = [comp(i).as, comp(i).a(setcuri)];
    end
    [comp(i).setcurs, I] = sort(comp(i).setcurs);
    comp(i).as = comp(i).as(I);
end
for i = 1:length(nocomp)
    nocomp(i).setcurs = [];
    nocomp(i).as = [];
    for setcuri = 1:length(nocomp(i).setcur)
        P = polyfit(nocomp(i).t{setcuri}, nocomp(i).pos{setcuri} / 8192, 2);
        nocomp(i).a(setcuri) = 2*P(1);
        nocomp(i).setcurs = [nocomp(i).setcurs, nocomp(i).setcur{setcuri}*-(-1).^setcuri];
        nocomp(i).as = [nocomp(i).as, nocomp(i).a(setcuri)];
    end
    [nocomp(i).setcurs, I] = sort(nocomp(i).setcurs);
    nocomp(i).as = nocomp(i).as(I);
end

%% plot acc vs cur
figure(2);clf;
allp = [];
for dat = [nocomp, comp]
    p = plot(dat.setcurs, dat.as, dat.linestyle, 'LineWidth', 2);
    hold on;
    allp = [allp, p];
end

%%
ivals = linspace(-dat.setcur{end}, dat.setcur{end});
% I = 1.00E-04; % kg.m^2
I = 2.5E-4;
Tvals = ivals / 150 * 8.269933431; % Nm
avals = Tvals / I / 2 / pi;
p = plot(ivals, avals, 'k--');
allp = [allp, p];
xlabel('Setpoint Current (A)');
ylabel('Angular acceleration ($rad/s^2$)');

% legend show
legend([allp(1), allp(4), allp(end)], ...
       'Before Compensation', 'After Compensation', 'Ideal',...
       'Location', 'SouthEast');
grid on
title('Torque vs Current (vs Compensation)');