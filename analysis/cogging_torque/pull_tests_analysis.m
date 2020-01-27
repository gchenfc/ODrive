%% Gerry Chen
% please note, in this data, only the passive motor was anticog compensated
clear
set(0,'defaultAxesFontSize',20)
set(0,'defaulttextInterpreter','latex');
set(groot, 'defaultAxesTickLabelInterpreter','latex');
set(groot, 'defaultLegendInterpreter','latex');

%% load stuff
load pull_tests/comp_fast_0
comp_fast = extract_data(data, 'comp fast');
load pull_tests/comp_slow_0
comp_slow = extract_data(data, 'comp slow');
load pull_tests/uncomp_fast_0
uncomp_fast = extract_data(data, 'uncomp fast');
load pull_tests/uncomp_slow_0
uncomp_slow = extract_data(data, 'uncomp slow');

%% plot raw
figure(1); clf;
% fast pos
ax1 = subplot(2,2,1);
for comp = [uncomp_fast, comp_fast]
    plot(comp.t, comp.vel, '-', 'DisplayName', comp.label);
    hold on;
end
xlabel('t (s)');
ylabel('v (rev/s)');
title('fast motion (10 rev/s)');
legend('Location', 'SouthEast')
% fast cur
ax3 = subplot(2,2,3);
for comp = [uncomp_fast, comp_fast]
    plot(comp.t, comp.cur, '-', 'DisplayName', comp.label);
    hold on;
end
xlabel('t (s)');
ylabel('current (A)');
legend show
% slow pos
ax2 = subplot(2,2,2);
for comp = [uncomp_slow, comp_slow]
    plot(comp.t, comp.vel, '-', 'DisplayName', comp.label);
    hold on;
end
xlabel('t (s)');
ylabel('v (rev/s)');
title('slow motion (1 rev/s)');
legend show
% slow cur
ax4 = subplot(2,2,4);
for comp = [uncomp_slow, comp_slow]
    plot(comp.t, comp.cur, '-', 'DisplayName', comp.label);
    hold on;
end
xlabel('t (s)');
ylabel('current (A)');
legend show
% all
linkaxes([ax1, ax3], 'x');
linkaxes([ax2, ax4], 'x');
ax1.XLim = [0, 4];
ax2.XLim = [1, 30];
sgtitle({'Pulling against a passive motor',...
         'passive anticogging comparison'}, 'FontSize', 30)

% print -dpng pull_tests/summary.png
     
%% util
function [obj] = extract_data(data, label)
    obj.t = data(1,:)' - data(1,1);
    obj.pos = (data(2,:)' - data(2,2)) / 8192;
    obj.vel = gradient(obj.pos) ./ gradient(obj.t);
    obj.vel = smooth(obj.vel, 21, 'sgolay');
    obj.cur = data(3,:)' - data(3,3);
    obj.label = label;
end