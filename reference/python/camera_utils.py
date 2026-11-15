# Copyright 2022 the Regents of the University of California, Nerfstudio Team and contributors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Camera transformation helper code.

REFERENCE IMPLEMENTATION -- nothing imports this module.

It is kept, deliberately, as the specification the native pipeline was checked
against. `orientation_method` and `center_method` are all implemented natively
now; `src/data/parsers/DatasetCommon.cpp::compute_normalized_transform` is the
C++ side, and its `gsplat` branch (`gsplat_rigid_transform`) is the port of
`orient_and_center_poses_gsplat()` below, with the deliberate differences listed
in `docs/notes/pose-normalization.md`. This file is the reference for both:

  * `auto_orient_and_center_poses()`  -- orientation_method in
    {pca, up, vertical, none} x center_method in {poses, focus, none}, plus
    the helpers it needs (`focus_of_attention`, `rotation_matrix_between`).
  * `orient_and_center_poses_gsplat()` -- the "gsplat" value of both options,
    plus `similarity_from_cameras`, `align_principal_axes`, `transform_points`,
    `transform_cameras`, `normalize`. This one also returns a scene scale,
    which is why the caller treats it separately.

The *caller* matters as much as these functions: how their output becomes
`transform_matrix` / `scale_factor` / `train_frame_scale` /
`train_to_normalized_transform`, and how `auto_scale_poses` and `train_frame`
interact with them, is written up with the original call-site code in
`docs/notes/pose-normalization.md`.

Everything above `normalize()` (quaternion helpers, `get_interpolated_poses*`,
`get_ordered_poses_and_k`) is camera-path interpolation, unrelated to either
option and already unused before this module left the code path. Left in place
rather than trimmed, so the orientation/centering code stays byte-identical to
the version that was verified against the native parser.
"""

import math
from typing import List, Literal, Optional, Tuple

import numpy as np
import torch
from jaxtyping import Float
from numpy.typing import NDArray
from torch import Tensor

_EPS = np.finfo(float).eps * 4.0


def unit_vector(data: NDArray, axis: Optional[int] = None) -> np.ndarray:
    """Return ndarray normalized by length, i.e. Euclidean norm, along axis.

    Args:
        axis: the axis along which to normalize into unit vector
        out: where to write out the data to. If None, returns a new np ndarray
    """
    data = np.array(data, dtype=np.float64, copy=True)
    if data.ndim == 1:
        data /= math.sqrt(np.dot(data, data))
        return data
    length = np.atleast_1d(np.sum(data * data, axis))
    np.sqrt(length, length)
    if axis is not None:
        length = np.expand_dims(length, axis)
    data /= length
    return data


def quaternion_from_matrix(matrix: NDArray, isprecise: bool = False) -> np.ndarray:
    """Return quaternion from rotation matrix.

    Args:
        matrix: rotation matrix to obtain quaternion
        isprecise: if True, input matrix is assumed to be precise rotation matrix and a faster algorithm is used.
    """
    M = np.array(matrix, dtype=np.float64, copy=True)[:4, :4]
    if isprecise:
        q = np.empty((4,))
        t = np.trace(M)
        if t > M[3, 3]:
            q[0] = t
            q[3] = M[1, 0] - M[0, 1]
            q[2] = M[0, 2] - M[2, 0]
            q[1] = M[2, 1] - M[1, 2]
        else:
            i, j, k = 1, 2, 3
            if M[1, 1] > M[0, 0]:
                i, j, k = 2, 3, 1
            if M[2, 2] > M[i, i]:
                i, j, k = 3, 1, 2
            t = M[i, i] - (M[j, j] + M[k, k]) + M[3, 3]
            q[i] = t
            q[j] = M[i, j] + M[j, i]
            q[k] = M[k, i] + M[i, k]
            q[3] = M[k, j] - M[j, k]
        q *= 0.5 / math.sqrt(t * M[3, 3])
    else:
        m00 = M[0, 0]
        m01 = M[0, 1]
        m02 = M[0, 2]
        m10 = M[1, 0]
        m11 = M[1, 1]
        m12 = M[1, 2]
        m20 = M[2, 0]
        m21 = M[2, 1]
        m22 = M[2, 2]
        # symmetric matrix K
        K = [
            [m00 - m11 - m22, 0.0, 0.0, 0.0],
            [m01 + m10, m11 - m00 - m22, 0.0, 0.0],
            [m02 + m20, m12 + m21, m22 - m00 - m11, 0.0],
            [m21 - m12, m02 - m20, m10 - m01, m00 + m11 + m22],
        ]
        K = np.array(K)
        K /= 3.0
        # quaternion is eigenvector of K that corresponds to largest eigenvalue
        w, V = np.linalg.eigh(K)
        q = V[np.array([3, 0, 1, 2]), np.argmax(w)]
    if q[0] < 0.0:
        np.negative(q, q)
    return q


def quaternion_slerp(
    quat0: NDArray, quat1: NDArray, fraction: float, spin: int = 0, shortestpath: bool = True
) -> np.ndarray:
    """Return spherical linear interpolation between two quaternions.
    Args:
        quat0: first quaternion
        quat1: second quaternion
        fraction: how much to interpolate between quat0 vs quat1 (if 0, closer to quat0; if 1, closer to quat1)
        spin: how much of an additional spin to place on the interpolation
        shortestpath: whether to return the short or long path to rotation
    """
    q0 = unit_vector(quat0[:4])
    q1 = unit_vector(quat1[:4])
    if q0 is None or q1 is None:
        raise ValueError("Input quaternions invalid.")
    if fraction == 0.0:
        return q0
    if fraction == 1.0:
        return q1
    d = np.dot(q0, q1)
    if abs(abs(d) - 1.0) < _EPS:
        return q0
    if shortestpath and d < 0.0:
        # invert rotation
        d = -d
        np.negative(q1, q1)
    angle = math.acos(d) + spin * math.pi
    if abs(angle) < _EPS:
        return q0
    isin = 1.0 / math.sin(angle)
    q0 *= math.sin((1.0 - fraction) * angle) * isin
    q1 *= math.sin(fraction * angle) * isin
    q0 += q1
    return q0


def quaternion_matrix(quaternion: NDArray) -> np.ndarray:
    """Return homogeneous rotation matrix from quaternion.

    Args:
        quaternion: value to convert to matrix
    """
    q = np.array(quaternion, dtype=np.float64, copy=True)
    n = np.dot(q, q)
    if n < _EPS:
        return np.identity(4)
    q *= math.sqrt(2.0 / n)
    q = np.outer(q, q)
    return np.array(
        [
            [1.0 - q[2, 2] - q[3, 3], q[1, 2] - q[3, 0], q[1, 3] + q[2, 0], 0.0],
            [q[1, 2] + q[3, 0], 1.0 - q[1, 1] - q[3, 3], q[2, 3] - q[1, 0], 0.0],
            [q[1, 3] - q[2, 0], q[2, 3] + q[1, 0], 1.0 - q[1, 1] - q[2, 2], 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ]
    )


def get_interpolated_poses(pose_a: NDArray, pose_b: NDArray, steps: int = 10) -> List[float]:
    """Return interpolation of poses with specified number of steps.
    Args:
        pose_a: first pose
        pose_b: second pose
        steps: number of steps the interpolated pose path should contain
    """

    quat_a = quaternion_from_matrix(pose_a[:3, :3])
    quat_b = quaternion_from_matrix(pose_b[:3, :3])

    ts = np.linspace(0, 1, steps)
    quats = [quaternion_slerp(quat_a, quat_b, t) for t in ts]
    trans = [(1 - t) * pose_a[:3, 3] + t * pose_b[:3, 3] for t in ts]

    poses_ab = []
    for quat, tran in zip(quats, trans):
        pose = np.identity(4)
        pose[:3, :3] = quaternion_matrix(quat)[:3, :3]
        pose[:3, 3] = tran
        poses_ab.append(pose[:3])
    return poses_ab


def get_interpolated_k(
    k_a: Float[Tensor, "3 3"], k_b: Float[Tensor, "3 3"], steps: int = 10
) -> List[Float[Tensor, "3 4"]]:
    """
    Returns interpolated path between two camera poses with specified number of steps.

    Args:
        k_a: camera matrix 1
        k_b: camera matrix 2
        steps: number of steps the interpolated pose path should contain

    Returns:
        List of interpolated camera poses
    """
    Ks: List[Float[Tensor, "3 3"]] = []
    ts = np.linspace(0, 1, steps)
    for t in ts:
        new_k = k_a * (1.0 - t) + k_b * t
        Ks.append(new_k)
    return Ks


def get_ordered_poses_and_k(
    poses: Float[Tensor, "num_poses 3 4"],
    Ks: Float[Tensor, "num_poses 3 3"],
) -> Tuple[Float[Tensor, "num_poses 3 4"], Float[Tensor, "num_poses 3 3"]]:
    """
    Returns ordered poses and intrinsics by euclidian distance between poses.

    Args:
        poses: list of camera poses
        Ks: list of camera intrinsics

    Returns:
        tuple of ordered poses and intrinsics

    """

    poses_num = len(poses)

    ordered_poses = torch.unsqueeze(poses[0], 0)
    ordered_ks = torch.unsqueeze(Ks[0], 0)

    # remove the first pose from poses
    poses = poses[1:]
    Ks = Ks[1:]

    for _ in range(poses_num - 1):
        distances = torch.norm(ordered_poses[-1][:, 3] - poses[:, :, 3], dim=1)
        idx = torch.argmin(distances)
        ordered_poses = torch.cat((ordered_poses, torch.unsqueeze(poses[idx], 0)), dim=0)
        ordered_ks = torch.cat((ordered_ks, torch.unsqueeze(Ks[idx], 0)), dim=0)
        poses = torch.cat((poses[0:idx], poses[idx + 1 :]), dim=0)
        Ks = torch.cat((Ks[0:idx], Ks[idx + 1 :]), dim=0)

    return ordered_poses, ordered_ks


def get_interpolated_poses_many(
    poses: Float[Tensor, "num_poses 3 4"],
    Ks: Float[Tensor, "num_poses 3 3"],
    steps_per_transition: int = 10,
    order_poses: bool = False,
) -> Tuple[Float[Tensor, "num_poses 3 4"], Float[Tensor, "num_poses 3 3"]]:
    """Return interpolated poses for many camera poses.

    Args:
        poses: list of camera poses
        Ks: list of camera intrinsics
        steps_per_transition: number of steps per transition
        order_poses: whether to order poses by euclidian distance

    Returns:
        tuple of new poses and intrinsics
    """
    traj = []
    k_interp = []

    if order_poses:
        poses, Ks = get_ordered_poses_and_k(poses, Ks)

    for idx in range(poses.shape[0] - 1):
        pose_a = poses[idx].cpu().numpy()
        pose_b = poses[idx + 1].cpu().numpy()
        poses_ab = get_interpolated_poses(pose_a, pose_b, steps=steps_per_transition)
        traj += poses_ab
        k_interp += get_interpolated_k(Ks[idx], Ks[idx + 1], steps=steps_per_transition)

    traj = np.stack(traj, axis=0)
    k_interp = torch.stack(k_interp, dim=0)

    return torch.tensor(traj, dtype=torch.float32), torch.tensor(k_interp, dtype=torch.float32)


def normalize(x: torch.Tensor) -> Float[Tensor, "*batch"]:
    """Returns a normalized vector."""
    return x / torch.linalg.norm(x)



def rotation_matrix_between(a: Float[Tensor, "3"], b: Float[Tensor, "3"]) -> Float[Tensor, "3 3"]:
    """Compute the rotation matrix that rotates vector a to vector b.

    Args:
        a: The vector to rotate.
        b: The vector to rotate to.
    Returns:
        The rotation matrix.
    """
    a = a / torch.linalg.norm(a)
    b = b / torch.linalg.norm(b)
    v = torch.linalg.cross(a, b)  # Axis of rotation.

    # Handle cases where `a` and `b` are parallel.
    eps = 1e-6
    if torch.sum(torch.abs(v)) < eps:
        x = torch.tensor([1.0, 0, 0]) if abs(a[0]) < eps else torch.tensor([0, 1.0, 0])
        v = torch.linalg.cross(a, x)

    v = v / torch.linalg.norm(v)
    skew_sym_mat = torch.Tensor(
        [
            [0, -v[2], v[1]],
            [v[2], 0, -v[0]],
            [-v[1], v[0], 0],
        ]
    )
    theta = torch.acos(torch.clip(torch.dot(a, b), -1, 1))

    # Rodrigues rotation formula. https://en.wikipedia.org/wiki/Rodrigues%27_rotation_formula
    return torch.eye(3) + torch.sin(theta) * skew_sym_mat + (1 - torch.cos(theta)) * (skew_sym_mat @ skew_sym_mat)


def focus_of_attention(poses: Float[Tensor, "*num_poses 4 4"], initial_focus: Float[Tensor, "3"]) -> Float[Tensor, "3"]:
    """Compute the focus of attention of a set of cameras. Only cameras
    that have the focus of attention in front of them are considered.

     Args:
        poses: The poses to orient.
        initial_focus: The 3D point views to decide which cameras are initially activated.

    Returns:
        The 3D position of the focus of attention.
    """
    # References to the same method in third-party code:
    # https://github.com/google-research/multinerf/blob/1c8b1c552133cdb2de1c1f3c871b2813f6662265/internal/camera_utils.py#L145
    # https://github.com/bmild/nerf/blob/18b8aebda6700ed659cb27a0c348b737a5f6ab60/load_llff.py#L197
    active_directions = -poses[:, :3, 2:3]
    active_origins = poses[:, :3, 3:4]
    # initial value for testing if the focus_pt is in front or behind
    focus_pt = initial_focus
    # Prune cameras which have the current have the focus_pt behind them.
    active = torch.sum(active_directions.squeeze(-1) * (focus_pt - active_origins.squeeze(-1)), dim=-1) > 0
    done = False
    # We need at least two active cameras, else fallback on the previous solution.
    # This may be the "poses" solution if no cameras are active on first iteration, e.g.
    # they are in an outward-looking configuration.
    while torch.sum(active.int()) > 1 and not done:
        active_directions = active_directions[active]
        active_origins = active_origins[active]
        # https://en.wikipedia.org/wiki/Line–line_intersection#In_more_than_two_dimensions
        m = torch.eye(3) - active_directions * torch.transpose(active_directions, -2, -1)
        mt_m = torch.transpose(m, -2, -1) @ m
        focus_pt = torch.linalg.inv(mt_m.mean(0)) @ (mt_m @ active_origins).mean(0)[:, 0]
        active = torch.sum(active_directions.squeeze(-1) * (focus_pt - active_origins.squeeze(-1)), dim=-1) > 0
        if active.all():
            # the set of active cameras did not change, so we're done.
            done = True
    return focus_pt


def auto_orient_and_center_poses(
    poses: Float[Tensor, "*num_poses 4 4"],
    method: Literal["pca", "up", "vertical", "none"] = "up",
    center_method: Literal["poses", "focus", "none"] = "poses",
) -> Tuple[Float[Tensor, "*num_poses 3 4"], Float[Tensor, "3 4"]]:
    """Orients and centers the poses.

    We provide three methods for orientation:

    - pca: Orient the poses so that the principal directions of the camera centers are aligned
        with the axes, Z corresponding to the smallest principal component.
        This method works well when all of the cameras are in the same plane, for example when
        images are taken using a mobile robot.
    - up: Orient the poses so that the average up vector is aligned with the z axis.
        This method works well when images are not at arbitrary angles.
    - vertical: Orient the poses so that the Z 3D direction projects close to the
        y axis in images. This method works better if cameras are not all
        looking in the same 3D direction, which may happen in camera arrays or in LLFF.

    There are two centering methods:

    - poses: The poses are centered around the origin.
    - focus: The origin is set to the focus of attention of all cameras (the
        closest point to cameras optical axes). Recommended for inward-looking
        camera configurations.

    Args:
        poses: The poses to orient.
        method: The method to use for orientation.
        center_method: The method to use to center the poses.

    Returns:
        Tuple of the oriented poses and the transform matrix.
    """

    origins = poses[..., :3, 3]

    mean_origin = torch.mean(origins, dim=0)
    translation_diff = origins - mean_origin

    if center_method == "poses":
        translation = mean_origin
    elif center_method == "focus":
        translation = focus_of_attention(poses, mean_origin)
    elif center_method == "none":
        translation = torch.zeros_like(mean_origin)
    else:
        raise ValueError(f"Unknown value for center_method: {center_method}")

    if method == "pca":
        _, eigvec = torch.linalg.eigh(translation_diff.T @ translation_diff)
        eigvec = torch.flip(eigvec, dims=(-1,))

        if torch.linalg.det(eigvec) < 0:
            eigvec[:, 2] = -eigvec[:, 2]

        transform = torch.cat([eigvec, eigvec @ -translation[..., None]], dim=-1)
        oriented_poses = transform @ poses

        if oriented_poses.mean(dim=0)[2, 1] < 0:
            oriented_poses[1:3, :] = -1 * oriented_poses[1:3, :]
            transform[1:3, :] = -1 * transform[1:3, :]
    elif method in ("up", "vertical"):
        up = torch.mean(poses[:, :3, 1], dim=0)
        up = up / torch.linalg.norm(up)
        if method == "vertical":
            # If cameras are not all parallel (e.g. not in an LLFF configuration),
            # we can find the 3D direction that most projects vertically in all
            # cameras by minimizing ||Xu|| s.t. ||u||=1. This total least squares
            # problem is solved by SVD.
            x_axis_matrix = poses[:, :3, 0]
            _, S, Vh = torch.linalg.svd(x_axis_matrix, full_matrices=False)
            # Singular values are S_i=||Xv_i|| for each right singular vector v_i.
            # ||S|| = sqrt(n) because lines of X are all unit vectors and the v_i
            # are an orthonormal basis.
            # ||Xv_i|| = sqrt(sum(dot(x_axis_j,v_i)^2)), thus S_i/sqrt(n) is the
            # RMS of cosines between x axes and v_i. If the second smallest singular
            # value corresponds to an angle error less than 10° (cos(80°)=0.17),
            # this is probably a degenerate camera configuration (typical values
            # are around 5° average error for the true vertical). In this case,
            # rather than taking the vector corresponding to the smallest singular
            # value, we project the "up" vector on the plane spanned by the two
            # best singular vectors. We could also just fallback to the "up"
            # solution.
            if S[1] > 0.17 * math.sqrt(poses.shape[0]):
                # regular non-degenerate configuration
                up_vertical = Vh[2, :]
                # It may be pointing up or down. Use "up" to disambiguate the sign.
                up = up_vertical if torch.dot(up_vertical, up) > 0 else -up_vertical
            else:
                # Degenerate configuration: project "up" on the plane spanned by
                # the last two right singular vectors (which are orthogonal to the
                # first). v_0 is a unit vector, no need to divide by its norm when
                # projecting.
                up = up - Vh[0, :] * torch.dot(up, Vh[0, :])
                # re-normalize
                up = up / torch.linalg.norm(up)

        rotation = rotation_matrix_between(up, torch.Tensor([0, 0, 1]))
        transform = torch.cat([rotation, rotation @ -translation[..., None]], dim=-1)
        oriented_poses = transform @ poses
    elif method == "none":
        transform = torch.eye(4)
        transform[:3, 3] = -translation
        transform = transform[:3, :]
        oriented_poses = transform @ poses
    else:
        raise ValueError(f"Unknown value for method: {method}")

    return oriented_poses, transform


# Code below is from gsplat, as an academic baseline

def similarity_from_cameras(c2w, strict_scaling=False, center_method="focus"):
    """
    reference: nerf-factory
    Get a similarity transform to normalize dataset
    from c2w (OpenCV convention) cameras
    :param c2w: (N, 4)
    :return T (4,4) , scale (float)
    """
    t = c2w[:, :3, 3]
    R = c2w[:, :3, :3]

    # (1) Rotate the world so that z+ is the up axis
    # we estimate the up axis by averaging the camera up axes
    ups = np.sum(R * np.array([0, -1.0, 0]), axis=-1)
    world_up = np.mean(ups, axis=0)
    world_up /= np.linalg.norm(world_up)

    up_camspace = np.array([0.0, -1.0, 0.0])
    c = (up_camspace * world_up).sum()
    cross = np.cross(world_up, up_camspace)
    skew = np.array(
        [
            [0.0, -cross[2], cross[1]],
            [cross[2], 0.0, -cross[0]],
            [-cross[1], cross[0], 0.0],
        ]
    )
    if c > -1:
        R_align = np.eye(3) + skew + (skew @ skew) * 1 / (1 + c)
    else:
        # In the unlikely case the original data has y+ up axis,
        # rotate 180-deg about x axis
        R_align = np.array([[-1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]])

    #  R_align = np.eye(3) # DEBUG
    R = R_align @ R
    fwds = np.sum(R * np.array([0, 0.0, 1.0]), axis=-1)
    t = (R_align @ t[..., None])[..., 0]

    # (2) Recenter the scene.
    if center_method == "focus":
        # find the closest point to the origin for each camera's center ray
        nearest = t + (fwds * -t).sum(-1)[:, None] * fwds
        translate = -np.median(nearest, axis=0)
    elif center_method == "poses":
        # use center of the camera positions
        translate = -np.median(t, axis=0)
    else:
        raise ValueError(f"Unknown center_method {center_method}")

    transform = np.eye(4)
    transform[:3, 3] = translate
    transform[:3, :3] = R_align

    # (3) Rescale the scene using camera distances
    scale_fn = np.max if strict_scaling else np.median
    scale = 1.0 / scale_fn(np.linalg.norm(t + translate, axis=-1))
    transform[:3, :] *= scale

    return transform


def align_principal_axes(point_cloud):
    # Compute centroid
    centroid = np.median(point_cloud, axis=0)

    # Translate point cloud to centroid
    translated_point_cloud = point_cloud - centroid

    # Compute covariance matrix
    covariance_matrix = np.cov(translated_point_cloud, rowvar=False)

    # Compute eigenvectors and eigenvalues
    eigenvalues, eigenvectors = np.linalg.eigh(covariance_matrix)

    # Sort eigenvectors by eigenvalues (descending order) so that the z-axis
    # is the principal axis with the smallest eigenvalue.
    sort_indices = eigenvalues.argsort()[::-1]
    eigenvectors = eigenvectors[:, sort_indices]

    # Check orientation of eigenvectors. If the determinant of the eigenvectors is
    # negative, then we need to flip the sign of one of the eigenvectors.
    if np.linalg.det(eigenvectors) < 0:
        eigenvectors[:, 0] *= -1

    # Create rotation matrix
    rotation_matrix = eigenvectors.T

    # Create SE(3) matrix (4x4 transformation matrix)
    transform = np.eye(4)
    transform[:3, :3] = rotation_matrix
    transform[:3, 3] = -rotation_matrix @ centroid

    return transform


def transform_points(matrix, points):
    """Transform points using an SE(3) matrix.

    Args:
        matrix: 4x4 SE(3) matrix
        points: Nx3 array of points

    Returns:
        Nx3 array of transformed points
    """
    assert matrix.shape == (4, 4)
    assert len(points.shape) == 2 and points.shape[1] == 3
    return points @ matrix[:3, :3].T + matrix[:3, 3]


def transform_cameras(matrix, camtoworlds):
    """Transform cameras using an SE(3) matrix.

    Args:
        matrix: 4x4 SE(3) matrix
        camtoworlds: Nx4x4 array of camera-to-world matrices

    Returns:
        Nx4x4 array of transformed camera-to-world matrices
    """
    assert matrix.shape == (4, 4)
    assert len(camtoworlds.shape) == 3 and camtoworlds.shape[1:] == (4, 4)
    camtoworlds = np.einsum("nij, ki -> nkj", camtoworlds, matrix)
    scaling = np.linalg.norm(camtoworlds[:, 0, :3], axis=1)
    camtoworlds[:, :3, :3] = camtoworlds[:, :3, :3] / scaling[:, None, None]
    return camtoworlds


def normalize(camtoworlds, points=None):
    T1 = similarity_from_cameras(camtoworlds)
    camtoworlds = transform_cameras(T1, camtoworlds)
    if points is not None:
        points = transform_points(T1, points)
        T2 = align_principal_axes(points)
        camtoworlds = transform_cameras(T2, camtoworlds)
        points = transform_points(T2, points)
        return camtoworlds, points, T2 @ T1
    else:
        return camtoworlds, T1

def orient_and_center_poses_gsplat(
    camtoworlds, points
):
    T1 = similarity_from_cameras(camtoworlds)
    camtoworlds = transform_cameras(T1, camtoworlds)
    points = transform_points(T1, points)

    T2 = align_principal_axes(points)
    camtoworlds = transform_cameras(T2, camtoworlds)
    points = transform_points(T2, points)

    transform = T2 @ T1

    # Fix for up side down. We assume more points towards
    # the bottom of the scene which is true when ground floor is
    # present in the images.
    if np.median(points[:, 2]) > np.mean(points[:, 2]):
        # rotate 180 degrees around x axis such that z is flipped
        T3 = np.array(
            [
                [1.0, 0.0, 0.0, 0.0],
                [0.0, -1.0, 0.0, 0.0],
                [0.0, 0.0, -1.0, 0.0],
                [0.0, 0.0, 0.0, 1.0],
            ]
        )
        camtoworlds = transform_cameras(T3, camtoworlds)
        points = transform_points(T3, points)
        transform = T3 @ transform

    camera_locations = camtoworlds[:, :3, 3]
    scene_center = np.mean(camera_locations, axis=0)
    dists = np.linalg.norm(camera_locations - scene_center, axis=1)
    scene_scale = 1.1 * np.max(dists)
    
    return camtoworlds, transform, scene_scale
