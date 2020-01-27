clear
set(0,'defaultAxesFontSize',20)
set(0,'defaulttextInterpreter','latex');
set(groot, 'defaultAxesTickLabelInterpreter','latex');
set(groot, 'defaultLegendInterpreter','latex');
folder = 'slow2_slower/';

%% load
allcomp = [];
allnocomp = [];
files = dir(folder);
for file = files'
    if endsWith(file.name, '.mat') && ...
       contains(file.name, 'comp') && ...
       ...~contains(file.name, 'no')% && ...
       ~contains(file.name, 'crosstalk')
        load([file.folder, '/', file.name]);
        obj = extract_data(data, strrep(file.name, '_', ' '));
        if contains(file.name, 'nocomp')
            obj.label = 'Before Compensation';
            allnocomp = [allnocomp, obj];
        else
            obj.label = 'After Compensation';
            allcomp = [allcomp, obj];
        end
    end
end

%% plot
figure(1);clf;
for dat = allnocomp
    p1 = plot(dat.t, filloutliers(dat.errpos, nan),...
              'DisplayName', dat.label,...
              'Color', 'r');
    hold on
end
for dat = allcomp
    p2 = plot(dat.t, filloutliers(dat.errpos, nan), ...
              'DisplayName', dat.label, ...
              'Color', 'k');
    hold on
end
legend([p1, p2]);
ylim([-.02, .02])
xlabel('t (s)')
ylabel('Position error (rev)');
title('Tracking Error vs Anticogging Compensation (0.03 rev/s)');
grid on

% print -dpng slow2_slower/trackingerror_anticogging_slower

%% util
function [obj] = extract_data(data, label)
    obj.t = data(1,:)' - data(1,1);
    obj.pos = (data(2,:)') / 8192;
%     obj.setpos = linspace(0, 1, length(obj.t))';
    obj.setpos = smooth(obj.pos, 2001, 'sgolay');
    obj.errpos = obj.pos - obj.setpos;
    obj.vel = gradient(obj.pos) ./ gradient(obj.t);
    obj.vel = smooth(obj.vel, 21, 'sgolay');
    obj.cur = data(3,:)' - data(3,3);
    obj.label = label;
end