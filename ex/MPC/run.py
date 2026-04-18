import numpy as np
import matplotlib.pyplot as plt


from mpc import MPC, UnicycleModel, AckermanModel, MassDamperSpringModel, Circle, Rectangle

def circle_trajectory(t):
    omega = 1.0
    R = 5.0
    return np.array([R*np.cos(omega*t), R*np.sin(omega*t)])
    # pass

def main():
    start = np.array([0, 0, 0])
    goal = np.array([3.0, 1.0, 0.2])
    model = UnicycleModel(np.zeros((3, 1)), 0.1, 0.05, 0.06)
    # model = AckermanModel(np.zeros((3, 1)), 0.15, 0.06)
    dt = 0.1
    mpc = MPC(model, 5, dt)
    print("Calculating trajectory")
    path = []
    obstacles = []
    # TODO obstacles
    # obstacles.append(Circle(np.array([1.2, 0.2]), radius=0.1, safe_margin=0.2))
    # obstacles.append(Rectangle(np.array([1.2, 0.2]), width=0.5, height=0.5, orientationDeg=45, safe_margin=0.2))
    # solution, path, stats = mpc.run(start, goal, obstacles=obstacles)
    solution, path, stats = mpc.run(start, trajectory_2d=circle_trajectory)
    
    print("Solution")
    np.set_printoptions(precision=4)
    print(path)
    
    print("Plotting")
    mpc.plot_2d(path, stats, desired_trajectory=circle_trajectory)

# For Mass-Damper-spring system
def desired_trajectory(t):
    return 0.1 * np.sin(0.5 * t)

def main_mds():
    start = np.array([-0.3, -0.05])  # [x, x']
    dt = 0.1
    model = MassDamperSpringModel(start.copy(), dt=dt, m_mass=2, c=3, k=4)
    mpc = MPC(model, T=5, dt=dt)
    print("Calculating MDS trajectory")
    solution, path, stats = mpc.run(start, desired_trajectory=desired_trajectory, maxiter=200)
    print("Plotting")
    mpc.plot_mds(path, stats, desired_trajectory=desired_trajectory)
    
if __name__ == "__main__":
    main()
    # main_mds()

